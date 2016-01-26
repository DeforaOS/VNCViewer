/* $Id$ */
static char const _copyright[] =
"Copyright © 2006 Anthony Liguori <anthony@codemonkey.ws>\n"
"Copyright © 2015-2016 Pierre Pronchery <khorben@defora.org>";
/* This file is part of DeforaOS Desktop VNCViewer */
static char const _license[] =
"This software is originally based on the GTK VNC Widget library.\n"
"\n"
"Copyright © 2006 Anthony Liguori <anthony@codemonkey.ws>\n"
"\n"
"This library is free software; you can redistribute it and/or\n"
"modify it under the terms of the GNU Lesser General Public\n"
"License as published by the Free Software Foundation; either\n"
"version 2.0 of the License, or (at your option) any later version.\n"
"\n"
"This library is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n"
"Lesser General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU Lesser General Public\n"
"License along with this library; if not, write to the Free Software\n"
"Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA";
/* FIXME disable grabbing mouse/keyboard on disconnect!!1 */

#include <vncdisplay.h>
#include <vncutil.h>
#ifdef HAVE_PULSEAUDIO
#include <vncaudiopulse.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <glib.h>
#ifdef HAVE_GIOUNIX
#include <gio/gunixsocketaddress.h>
#endif

#if WITH_LIBVIEW
#include <libview/autoDrawer.h>
#endif

#include "vncviewer.h"

#include "../config.h"
#define _(string) gettext(string)

/* constants */
#ifndef PROGNAME
# define PROGNAME "vncviewer"
#endif


#ifndef GDK_Return
#define GDK_Return GDK_KEY_Return
#endif
#ifndef GDK_Escape
#define GDK_Escape GDK_KEY_Escape
#endif
#ifndef GDK_BackSpace
#define GDK_BackSpace GDK_KEY_BackSpace
#endif
#ifndef GDK_Delete
#define GDK_Delete GDK_KEY_Delete
#endif
#ifndef GDK_Control_L
#define GDK_Control_L GDK_KEY_Control_L
#endif
#ifndef GDK_Alt_L
#define GDK_Alt_L GDK_KEY_Alt_L
#endif
#ifndef GDK_F1
#define GDK_F1 GDK_KEY_F1
#endif
#ifndef GDK_F2
#define GDK_F2 GDK_KEY_F2
#endif
#ifndef GDK_F3
#define GDK_F3 GDK_KEY_F3
#endif
#ifndef GDK_F4
#define GDK_F4 GDK_KEY_F4
#endif
#ifndef GDK_F5
#define GDK_F5 GDK_KEY_F5
#endif
#ifndef GDK_F6
#define GDK_F6 GDK_KEY_F6
#endif
#ifndef GDK_F7
#define GDK_F7 GDK_KEY_F7
#endif
#ifndef GDK_F8
#define GDK_F8 GDK_KEY_F8
#endif
#ifndef GDK_F11
#define GDK_F11 GDK_KEY_F11
#endif

#ifdef HAVE_PULSEAUDIO
static VncAudioPulse *pa = NULL;
#endif

static GtkWidget *vnc = NULL;
static GtkWidget *status;
static GtkWidget *statusbar;

static char const * _authors[] = {
	"Anthony Liguori <anthony@codemonkey.ws>",
	"Pierre Pronchery <khorben@defora.org>",
	NULL
};

typedef struct {
    GtkWidget *label;
    guint curkeys;
    guint numkeys;
    guint *keysyms;
    gboolean set;
} VncGrabDefs;

gboolean enable_mnemonics_save;
GtkAccelGroup *accel_group;
gboolean accel_enabled = TRUE;
GValue accel_setting;
GSList *accel_list;

static void set_status(char const * format, ...)
{
	va_list ap;
	guint id;
	gchar *status;

	va_start(ap, format);
	if (vnc != NULL) {
		id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "");
		status = g_strdup_vprintf(format, ap);
		gtk_statusbar_pop(GTK_STATUSBAR(statusbar), id);
		gtk_statusbar_push(GTK_STATUSBAR(statusbar), id, status);
		g_free(status);
	} else {
	    fputs(PROGNAME ": ", stderr);
		vfprintf(stderr, format, ap);
	    fputc('\n', stderr);
	}
	va_end(ap);
}

static void set_title(VncDisplay *vncdisplay, GtkWidget *window,
                      gboolean grabbed)
{
    const gchar *name = vnc_display_get_name(VNC_DISPLAY(vncdisplay));
    VncGrabSequence *seq = vnc_display_get_grab_keys(vncdisplay);
    gchar *seqstr = vnc_grab_sequence_as_string(seq);
    gchar *title;

    if (grabbed) {
		if (name != NULL)
		    title = g_strdup_printf(
				    _("%s - %s (Press %s to release pointer)"),
				    PACKAGE, name, seqstr);
		else
		    title = g_strdup_printf(
				    _("%s (Press %s to release pointer)"),
				    PACKAGE, seqstr);
	}
    else if(name != NULL)
        title = g_strdup_printf("%s - %s", PACKAGE, name);
    else
        title = g_strdup(PACKAGE);

    gtk_window_set_title(GTK_WINDOW(window), title);

    g_free(seqstr);
    g_free(title);
}

static gboolean vnc_screenshot(GtkWidget *window G_GNUC_UNUSED,
                               GdkEvent *ev, GtkWidget *vncdisplay)
{
    const char filename[] = "vncviewer.png";
    GdkPixbuf * pix;

    if (ev->key.keyval == GDK_F11
		    && (pix = vnc_display_get_pixbuf(VNC_DISPLAY(vncdisplay)))
		    != NULL)
    {
        gdk_pixbuf_save(pix, filename, "png", NULL, "tEXt::Generator App", "vncviewer", NULL);
        g_object_unref(pix);
        set_status(_("Screenshot saved to %s"), filename);
    }
    return FALSE;
}


static void
vnc_disable_modifiers(GtkWindow *window)
{
    GtkSettings *settings = gtk_settings_get_default();
    GValue empty;
    GSList *accels;

    if (!accel_enabled)
        return;

    /* This stops F10 activating menu bar */
    memset(&empty, 0, sizeof empty);
    g_value_init(&empty, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(settings), "gtk-menu-bar-accel", &accel_setting);
    g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &empty);

    /* This stops global accelerators like Ctrl+Q == Quit */
    for (accels = accel_list ; accels ; accels = accels->next) {
        if (accel_group == accels->data)
            continue;
        gtk_window_remove_accel_group(GTK_WINDOW(window), accels->data);
    }

    /* This stops menu bar shortcuts like Alt+F == File */
    g_object_get(settings,
                 "gtk-enable-mnemonics", &enable_mnemonics_save,
                 NULL);
    g_object_set(settings,
                 "gtk-enable-mnemonics", FALSE,
                 NULL);

    accel_enabled = FALSE;
}


static void
vnc_enable_modifiers(GtkWindow *window)
{
    GtkSettings *settings = gtk_settings_get_default();
    GSList *accels;

    if (accel_enabled)
        return;

    /* This allows F10 activating menu bar */
    g_object_set_property(G_OBJECT(settings), "gtk-menu-bar-accel", &accel_setting);

    /* This allows global accelerators like Ctrl+Q == Quit */
    for (accels = accel_list ; accels ; accels = accels->next) {
        if (accel_group == accels->data)
            continue;
        gtk_window_add_accel_group(GTK_WINDOW(window), accels->data);
    }

    /* This allows menu bar shortcuts like Alt+F == File */
    g_object_set(settings,
                 "gtk-enable-mnemonics", enable_mnemonics_save,
                 NULL);

    accel_enabled = TRUE;
}


static void vnc_key_grab(GtkWidget *vncdisplay G_GNUC_UNUSED, GtkWidget *window)
{
    vnc_disable_modifiers(GTK_WINDOW(window));
}

static void vnc_key_ungrab(GtkWidget *vncdisplay G_GNUC_UNUSED, GtkWidget *window)
{
    vnc_enable_modifiers(GTK_WINDOW(window));
}

static void vnc_mouse_grab(GtkWidget *vncdisplay, GtkWidget *window)
{
    set_title(VNC_DISPLAY(vncdisplay), window, TRUE);
}

static void vnc_mouse_ungrab(GtkWidget *vncdisplay, GtkWidget *window)
{
    set_title(VNC_DISPLAY(vncdisplay), window, FALSE);
}

static int connected = 0;

static void vnc_connected(GtkWidget *vncdisplay G_GNUC_UNUSED)
{
    set_status(_("Connected to server"));
    gtk_image_set_from_stock(GTK_IMAGE(status), GTK_STOCK_CONNECT,
		    GTK_ICON_SIZE_MENU);
#if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text(status, _("Connected"));
#endif
    connected = 1;
}

static void vnc_initialized(GtkWidget *vncdisplay, GtkWidget *window)
{
    set_status(_("Connection initialized"));
    set_title(VNC_DISPLAY(vncdisplay), window, FALSE);
    gtk_widget_show_all(window);

#ifdef HAVE_PULSEAUDIO
    VncConnection *conn;
    VncAudioFormat format = {
        VNC_AUDIO_FORMAT_RAW_S32,
        2,
        44100,
    };
    conn = vnc_display_get_connection(VNC_DISPLAY(vncdisplay));
    vnc_connection_set_audio_format(conn, &format);
    vnc_connection_set_audio(conn, VNC_AUDIO(pa));
    vnc_connection_audio_enable(conn);
#endif
}

static void vnc_auth_failure(GtkWidget *vncdisplay G_GNUC_UNUSED,
                             const char *msg)
{
    set_status(_("Authentication failed '%s'"), msg ? msg : "");
}

static void vnc_desktop_resize(GtkWidget *vncdisplay G_GNUC_UNUSED,
                               int width, int height)
{
    set_status(_("Remote desktop size changed to %dx%d"), width, height);
}

static void vnc_disconnected(GtkWidget *vncdisplay G_GNUC_UNUSED,
		GtkWidget *window)
{
    if(connected)
        set_status(_("Disconnected from server"));
    else
        set_status(_("Failed to connect to server"));
    gtk_image_set_from_stock(GTK_IMAGE(status), GTK_STOCK_DISCONNECT,
		    GTK_ICON_SIZE_MENU);
#if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text(status, _("Disconnected"));
#endif
    gtk_widget_show_all(window);
    connected = 0;
}

static void send_caf1(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F1 };
    set_status(_("Sending Ctrl+Alt+F1"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf2(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F2 };
    set_status(_("Sending Ctrl+Alt+F2"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf3(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F3 };
    set_status(_("Sending Ctrl+Alt+F3"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf4(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F4 };
    set_status(_("Sending Ctrl+Alt+F4"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf5(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F5 };
    set_status(_("Sending Ctrl+Alt+F5"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf6(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F6 };
    set_status(_("Sending Ctrl+Alt+F6"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf7(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F7 };
    set_status(_("Sending Ctrl+Alt+F7"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_caf8(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_F8 };
    set_status(_("Sending Ctrl+Alt+F8"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_cad(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_Delete };
    set_status(_("Sending Ctrl+Alt+Delete"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void send_cab(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *vncdisplay)
{
    guint keys[] = { GDK_Control_L, GDK_Alt_L, GDK_BackSpace };
    set_status(_("Sending Ctrl+Alt+Backspace"));
    vnc_display_send_keys(VNC_DISPLAY(vncdisplay), keys,
                          sizeof(keys)/sizeof(keys[0]));
}

static void do_about(GtkWidget *menu, GtkWidget *window)
{
	GtkWidget *dialog;

	dialog = gtk_about_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), _authors);
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
			_("VNC client for the DeforaOS desktop"));
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), _copyright);
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog),
			"gnome-remote-desktop");
	gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(dialog), _license);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), PACKAGE);
#else
	gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), PACKAGE);
#endif
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), VERSION);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog),
			"http://www.defora.org/");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void do_fullscreen(GtkWidget *menu, GtkWidget *window)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu))) {
		gtk_widget_hide(statusbar);
		gtk_window_fullscreen(GTK_WINDOW(window));
	} else {
		gtk_window_unfullscreen(GTK_WINDOW(window));
		gtk_widget_show(statusbar);
	}
}

static void do_scaling(GtkWidget *menu, GtkWidget *vncdisplay)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu)))
        vnc_display_set_scaling(VNC_DISPLAY(vncdisplay), TRUE);
    else
        vnc_display_set_scaling(VNC_DISPLAY(vncdisplay), FALSE);
}

static void dialog_update_keysyms(GtkWidget *window, guint *keysyms, guint numsyms)
{
    gchar *keys;
    int i;

    keys = g_strdup("");
    for (i = 0; i < numsyms; i++)
            keys = g_strdup_printf("%s%s%s", keys,
                                   (strlen(keys) > 0) ? "+" : " ", gdk_keyval_name(keysyms[i]));

    gtk_label_set_text( GTK_LABEL(window), keys);
}

static gboolean dialog_key_ignore(int keyval)
{
    switch (keyval) {
    case GDK_Return:
    case GDK_Escape:
        return TRUE;

    default:
        return FALSE;
    }
}

static gboolean dialog_key_press(GtkWidget *window G_GNUC_UNUSED,
                                 GdkEvent *ev, VncGrabDefs *defs)
{
    gboolean keySymExists;
    int i;

    if (dialog_key_ignore(ev->key.keyval))
        return FALSE;

    if (defs->set && defs->curkeys)
        return FALSE;

    /* Check whether we already have keysym in array - i.e. it was handler by something else */
    keySymExists = FALSE;
    for (i = 0; i < defs->curkeys; i++) {
        if (defs->keysyms[i] == ev->key.keyval)
            keySymExists = TRUE;
    }

    if (!keySymExists) {
        defs->keysyms = g_renew(guint, defs->keysyms, defs->curkeys + 1);
        defs->keysyms[defs->curkeys] = ev->key.keyval;
        defs->curkeys++;
    }

    dialog_update_keysyms(defs->label, defs->keysyms, defs->curkeys);

    if (!ev->key.is_modifier) {
        defs->set = TRUE;
        defs->numkeys = defs->curkeys;
        defs->curkeys--;
    }

    return FALSE;
}

static gboolean dialog_key_release(GtkWidget *window G_GNUC_UNUSED,
                                   GdkEvent *ev, VncGrabDefs *defs)
{
    int i;

    if (dialog_key_ignore(ev->key.keyval))
        return FALSE;

    if (defs->set) {
        if (defs->curkeys == 0)
            defs->set = FALSE;
        if (defs->curkeys)
            defs->curkeys--;
        return FALSE;
    }

    for (i = 0; i < defs->curkeys; i++)
            if (defs->keysyms[i] == ev->key.keyval)
                {
                    defs->keysyms[i] = defs->keysyms[defs->curkeys - 1];
                    defs->curkeys--;
                    defs->keysyms = g_renew(guint, defs->keysyms, defs->curkeys);
                }

    dialog_update_keysyms(defs->label, defs->keysyms, defs->curkeys);

    return FALSE;
}

static void do_set_grab_keys(GtkWidget *menu G_GNUC_UNUSED, GtkWidget *window)
{
    VncGrabDefs *defs;
    VncGrabSequence *seq;
    GtkWidget *dialog, *content_area, *label;
    gint result;

    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
		    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		    GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
#if GTK_CHECK_VERSION(2, 6, 0)
		    "%s", _("Key recorder"));
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
		    _("Please press desired grab key combination"));
#if GTK_CHECK_VERSION(2, 10, 0)
    gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog),
		    gtk_image_new_from_stock(GTK_STOCK_MEDIA_RECORD,
			    GTK_ICON_SIZE_DIALOG));
#endif
    gtk_window_set_title(GTK_WINDOW(dialog), _("Key recorder"));

    label = gtk_label_new("");
    defs = g_new(VncGrabDefs, 1);
    defs->label = label;
    defs->keysyms = 0;
    defs->numkeys = 0;
    defs->curkeys = 0;
    defs->set = FALSE;
    g_signal_connect(dialog, "key-press-event",
                     G_CALLBACK(dialog_key_press), defs);
    g_signal_connect(dialog, "key-release-event",
                     G_CALLBACK(dialog_key_release), defs);
#if GTK_CHECK_VERSION(2, 14, 0)
    content_area = gtk_dialog_get_content_area( GTK_DIALOG(dialog) );
#else
    content_area = dialog->vbox;
#endif
    gtk_container_add( GTK_CONTAINER(content_area), label);
    gtk_widget_show_all(dialog);

    result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_ACCEPT) {
        /* Accepted so we make a grab sequence from it */
        seq = vnc_grab_sequence_new(defs->numkeys,
                                    defs->keysyms);

        vnc_display_set_grab_keys(VNC_DISPLAY(vnc), seq);
        set_title(VNC_DISPLAY(vnc), window, FALSE);
        vnc_grab_sequence_free(seq);
    }
    g_free(defs);
    gtk_widget_destroy(dialog);
}

static void vnc_credential(GtkWidget *vncdisplay, GValueArray *credList)
{
    GtkWidget *dialog = NULL;
    int response;
    unsigned int i, prompt = 0;
    const char **data;

    set_status(_("Got credential request for %d credential(s)"),
		    credList->n_values);

    data = g_new0(const char *, credList->n_values);

    for (i = 0 ; i < credList->n_values ; i++) {
        GValue *cred = g_value_array_get_nth(credList, i);
        switch (g_value_get_enum(cred)) {
        case VNC_DISPLAY_CREDENTIAL_USERNAME:
        case VNC_DISPLAY_CREDENTIAL_PASSWORD:
            prompt++;
            break;
        case VNC_DISPLAY_CREDENTIAL_CLIENTNAME:
            data[i] = PACKAGE;
        default:
            break;
        }
    }

    if (prompt) {
        GtkWidget **label, **entry, *box, *vbox;
        int row;
        dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_OK_CANCEL,
#if GTK_CHECK_VERSION(2, 6, 0)
		    "%s", _("Authentication required"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
		    "");
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog),
			gtk_image_new_from_stock(
				GTK_STOCK_DIALOG_AUTHENTICATION,
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_window_set_title(GTK_WINDOW(dialog), _("Authentication required"));
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

        box = gtk_table_new(credList->n_values, 2, FALSE);
        label = g_new(GtkWidget *, prompt);
        entry = g_new(GtkWidget *, prompt);

        for (i = 0, row =0 ; i < credList->n_values ; i++) {
            GValue *cred = g_value_array_get_nth(credList, i);
            entry[row] = gtk_entry_new();
            switch (g_value_get_enum(cred)) {
            case VNC_DISPLAY_CREDENTIAL_USERNAME:
                label[row] = gtk_label_new(_("Username: "));
                break;
            case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                label[row] = gtk_label_new(_("Password: "));
                gtk_entry_set_activates_default(GTK_ENTRY(entry[row]), TRUE);
                break;
            default:
                continue;
            }
            if (g_value_get_enum (cred) == VNC_DISPLAY_CREDENTIAL_PASSWORD)
                gtk_entry_set_visibility (GTK_ENTRY (entry[row]), FALSE);

            gtk_table_attach(GTK_TABLE(box), label[i], 0, 1, row, row+1, GTK_SHRINK, GTK_SHRINK, 3, 3);
            gtk_table_attach(GTK_TABLE(box), entry[i], 1, 2, row, row+1, GTK_SHRINK, GTK_SHRINK, 3, 3);
            row++;
        }

        vbox = gtk_bin_get_child(GTK_BIN(dialog));
        gtk_container_add(GTK_CONTAINER(vbox), box);

        gtk_widget_show_all(dialog);
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_hide(GTK_WIDGET(dialog));

        if (response == GTK_RESPONSE_OK) {
            for (i = 0, row = 0 ; i < credList->n_values ; i++) {
                GValue *cred = g_value_array_get_nth(credList, i);
                switch (g_value_get_enum(cred)) {
                case VNC_DISPLAY_CREDENTIAL_USERNAME:
                case VNC_DISPLAY_CREDENTIAL_PASSWORD:
                    data[i] = gtk_entry_get_text(GTK_ENTRY(entry[row]));
                    break;
                default:
                    continue;
                }
                row++;
            }
        }
    }

    for (i = 0 ; i < credList->n_values ; i++) {
        GValue *cred = g_value_array_get_nth(credList, i);
        if (data[i]) {
            if (vnc_display_set_credential(VNC_DISPLAY(vncdisplay),
                                           g_value_get_enum(cred),
                                           data[i])) {
                set_status(_("Failed to set credential type %d"),
				g_value_get_enum(cred));
                vnc_display_close(VNC_DISPLAY(vncdisplay));
            }
        } else {
            set_status(_("Unsupported credential type %d"),
			    g_value_get_enum(cred));
            vnc_display_close(VNC_DISPLAY(vncdisplay));
        }
    }

    g_free(data);
    if (dialog)
        gtk_widget_destroy(GTK_WIDGET(dialog));
}

#if WITH_LIBVIEW
static gboolean window_state_event(GtkWidget *widget,
                                   GdkEventWindowState *event,
                                   gpointer data)
{
    ViewAutoDrawer *drawer = VIEW_AUTODRAWER(data);

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
            vnc_display_force_grab(VNC_DISPLAY(vnc), TRUE);
            ViewAutoDrawer_SetActive(drawer, TRUE);
        } else {
            vnc_display_force_grab(VNC_DISPLAY(vnc), FALSE);
            ViewAutoDrawer_SetActive(drawer, FALSE);
        }
    }

    return FALSE;
}
#endif

int vncviewer(gchar ** args)
{
    GtkWidget *window;
    GtkWidget *layout;
    GtkWidget *menubar;
    GtkWidget *file, *sendkey, *view, *settings, *help;
    GtkWidget *submenu;
    GtkWidget *close;
    GtkWidget *caf1;
    GtkWidget *caf2;
    GtkWidget *caf3;
    GtkWidget *caf4;
    GtkWidget *caf5;
    GtkWidget *caf6;
    GtkWidget *caf7;
    GtkWidget *caf8;
    GtkWidget *cad;
    GtkWidget *cab;
    GtkWidget *fullscreen;
    GtkWidget *scaling;
    GtkWidget *about;
    GtkWidget *showgrabkeydlg;
    GtkWidget *widget;
    GSList *accels;
    gchar *tmp;
    gchar *hostname;
    gchar *port;

    vnc = vnc_display_new();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_icon_name(GTK_WINDOW(window), "gnome-remote-desktop");
#if WITH_LIBVIEW
    layout = ViewAutoDrawer_New();
#elif GTK_CHECK_VERSION(3, 0, 0)
    layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    layout = gtk_vbox_new(FALSE, 0);
#endif
    menubar = gtk_menu_bar_new();

#ifdef HAVE_PULSEAUDIO
    pa = vnc_audio_pulse_new();
#endif

    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    gtk_window_set_title(GTK_WINDOW(window), PACKAGE);

    file = gtk_menu_item_new_with_mnemonic(_("_File"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file);

    submenu = gtk_menu_new();
    close = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), close);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), submenu);

    sendkey = gtk_menu_item_new_with_mnemonic(_("_Send Key"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), sendkey);

    submenu = gtk_menu_new();

    caf1 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_1"));
    caf2 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_2"));
    caf3 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_3"));
    caf4 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_4"));
    caf5 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_5"));
    caf6 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_6"));
    caf7 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_7"));
    caf8 = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+F_8"));
    cad = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+_Del"));
    cab = gtk_menu_item_new_with_mnemonic(_("Ctrl+Alt+_Backspace"));

    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf1);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf2);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf3);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf4);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf5);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf6);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf7);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), caf8);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), cad);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), cab);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(sendkey), submenu);

    view = gtk_menu_item_new_with_mnemonic(_("_View"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view);

    submenu = gtk_menu_new();

    fullscreen = gtk_check_menu_item_new_with_mnemonic(_("_Full Screen"));
    scaling = gtk_check_menu_item_new_with_mnemonic(_("Scaled display"));

    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), fullscreen);
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), scaling);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), submenu);

    settings = gtk_menu_item_new_with_mnemonic(_("_Settings"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), settings);

    submenu = gtk_menu_new();

    showgrabkeydlg = gtk_menu_item_new_with_mnemonic(_("_Set grab keys"));
    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), showgrabkeydlg);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(settings), submenu);

    help = gtk_menu_item_new_with_mnemonic(_("_Help"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help);

    submenu = gtk_menu_new();

    about = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(submenu), about);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), submenu);

#if GTK_CHECK_VERSION(3, 0, 0)
    widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
#else
    widget = gtk_hbox_new(FALSE, 4);
#endif
    status = gtk_image_new_from_stock(GTK_STOCK_DISCONNECT, GTK_ICON_SIZE_MENU);
#if GTK_CHECK_VERSION(2, 12, 0)
    gtk_widget_set_tooltip_text(status, _("Disconnected"));
#endif
    gtk_box_pack_start(GTK_BOX(widget), status, FALSE, TRUE, 0);
    statusbar = gtk_statusbar_new();
#if !GTK_CHECK_VERSION(3, 0, 0)
    gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), TRUE);
#endif
    gtk_box_pack_start(GTK_BOX(widget), statusbar, TRUE, TRUE, 0);

#if WITH_LIBVIEW
    ViewAutoDrawer_SetActive(VIEW_AUTODRAWER(layout), FALSE);
    ViewOvBox_SetOver(VIEW_OV_BOX(layout), menubar);
    ViewOvBox_SetUnder(VIEW_OV_BOX(layout), vnc);
    ViewOvBox_SetUnder(VIEW_OV_BOX(layout), widget);
#else
    gtk_box_pack_start(GTK_BOX(layout), menubar, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layout), vnc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(layout), widget, FALSE, TRUE, 0);
#endif
    gtk_container_add(GTK_CONTAINER(window), layout);
    gtk_widget_realize(vnc);

    g_value_init(&accel_setting, G_TYPE_STRING);

    accels = gtk_accel_groups_from_object(G_OBJECT(window));

    for ( ; accels ; accels = accels->next) {
        accel_list = g_slist_append(accel_list, accels->data);
        g_object_ref(G_OBJECT(accels->data));
    }

#ifdef HAVE_GIOUNIX
    if (strchr(args[0], '/')) {
        GSocketAddress *addr = g_unix_socket_address_new_with_type
            (args[0], strlen(args[0]),
             G_UNIX_SOCKET_ADDRESS_PATH);

        vnc_display_open_addr(VNC_DISPLAY(vnc), addr, NULL);

        g_object_unref(addr);
    } else {
#endif
        if (g_str_equal(args[0], ""))
            hostname = g_strdup("127.0.0.1");
        else
            hostname = g_strdup(args[0]);

        tmp = strchr(hostname, ':');
        if (tmp) {
            *tmp = '\0';
            port = g_strdup_printf("%d", 5900 + atoi(tmp + 1));
        } else {
            port = g_strdup("5900");
        }
        vnc_display_open_host(VNC_DISPLAY(vnc), hostname, port);
        g_free(hostname);
        g_free(port);
#ifdef HAVE_GIOUNIX
    }
#endif
    vnc_display_set_keyboard_grab(VNC_DISPLAY(vnc), TRUE);
    vnc_display_set_pointer_grab(VNC_DISPLAY(vnc), TRUE);
    vnc_display_set_pointer_local(VNC_DISPLAY(vnc), TRUE);

    if (!gtk_widget_is_composited(window)) {
        vnc_display_set_scaling(VNC_DISPLAY(vnc), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(scaling), TRUE);
    }

    g_signal_connect(window, "delete-event",
                     G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(vnc, "vnc-connected",
                     G_CALLBACK(vnc_connected), NULL);
    g_signal_connect(vnc, "vnc-initialized",
                     G_CALLBACK(vnc_initialized), window);
    g_signal_connect(vnc, "vnc-disconnected",
                     G_CALLBACK(vnc_disconnected), window);
    g_signal_connect(vnc, "vnc-auth-credential",
                     G_CALLBACK(vnc_credential), NULL);
    g_signal_connect(vnc, "vnc-auth-failure",
                     G_CALLBACK(vnc_auth_failure), NULL);

    g_signal_connect(vnc, "vnc-desktop-resize",
                     G_CALLBACK(vnc_desktop_resize), NULL);

    g_signal_connect(vnc, "vnc-pointer-grab",
                     G_CALLBACK(vnc_mouse_grab), window);
    g_signal_connect(vnc, "vnc-pointer-ungrab",
                     G_CALLBACK(vnc_mouse_ungrab), window);

    g_signal_connect(vnc, "vnc-keyboard-grab",
                     G_CALLBACK(vnc_key_grab), window);
    g_signal_connect(vnc, "vnc-keyboard-ungrab",
                     G_CALLBACK(vnc_key_ungrab), window);


    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(vnc_screenshot), vnc);

    g_signal_connect(close, "activate",
                     G_CALLBACK(gtk_main_quit), vnc);

    g_signal_connect(caf1, "activate",
                     G_CALLBACK(send_caf1), vnc);
    g_signal_connect(caf2, "activate",
                     G_CALLBACK(send_caf2), vnc);
    g_signal_connect(caf3, "activate",
                     G_CALLBACK(send_caf3), vnc);
    g_signal_connect(caf4, "activate",
                     G_CALLBACK(send_caf4), vnc);
    g_signal_connect(caf5, "activate",
                     G_CALLBACK(send_caf5), vnc);
    g_signal_connect(caf6, "activate",
                     G_CALLBACK(send_caf6), vnc);
    g_signal_connect(caf7, "activate",
                     G_CALLBACK(send_caf7), vnc);
    g_signal_connect(caf8, "activate",
                     G_CALLBACK(send_caf8), vnc);
    g_signal_connect(cad, "activate",
                     G_CALLBACK(send_cad), vnc);
    g_signal_connect(cab, "activate",
                     G_CALLBACK(send_cab), vnc);
    g_signal_connect(showgrabkeydlg, "activate",
                     G_CALLBACK(do_set_grab_keys), window);
    g_signal_connect(fullscreen, "toggled",
                     G_CALLBACK(do_fullscreen), window);
    g_signal_connect(scaling, "toggled",
                     G_CALLBACK(do_scaling), vnc);
    g_signal_connect(about, "activate",
                     G_CALLBACK(do_about), window);
#if WITH_LIBVIEW
    g_signal_connect(window, "window-state-event",
                     G_CALLBACK(window_state_event), layout);
#endif
    return 0;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
