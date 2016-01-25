/* $Id$ */
/* Copyright (c) 2015 Pierre Pronchery <khorben@defora.org> */
/* This file is part of DeforaOS Desktop VNCViewer */
/* Originally based on:
 * GTK VNC Widget
 *
 * Copyright (c) 2006 Anthony Liguori <anthony@codemonkey.ws>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <gtk/gtk.h>
#include <vncdisplay.h>
#include <vncutil.h>
#include "vncviewer.h"
#include "../config.h"

/* constants */
#ifndef PROGNAME
# define PROGNAME "vncviewer"
#endif


static gchar **args = NULL;
static const GOptionEntry options [] =
    {
        {
            G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &args,
            NULL, "[hostname][:display]" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
    };


static char * _vncviewer_get_hostname(void)
{
	GtkWidget * dialog;
	GtkWidget * box;
	GtkWidget * widget;
	GtkWidget * content;
	char * hostname = NULL;

	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_NONE,
#if GTK_CHECK_VERSION(2, 6, 0)
			"%s", PACKAGE);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
#endif
			"Please choose a hostname to connect to");
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_message_dialog_set_image(GTK_MESSAGE_DIALOG(dialog),
			gtk_image_new_from_icon_name("gnome-remote-desktop",
				GTK_ICON_SIZE_DIALOG));
#endif
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CONNECT,
			GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),
			GTK_RESPONSE_ACCEPT);
	gtk_window_set_title(GTK_WINDOW(dialog), PACKAGE);
	box = gtk_hbox_new(FALSE, 4);
	widget = gtk_label_new("Hostname: ");
	gtk_box_pack_start(GTK_BOX(box), widget, FALSE, TRUE, 0);
	widget = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(widget), TRUE);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
	gtk_widget_show_all(box);
	content = gtk_bin_get_child(GTK_BIN(dialog));
	gtk_container_add(GTK_CONTAINER(content), box);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
		hostname = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
	gtk_widget_destroy(dialog);
	return hostname;
}


/* main */
int main(int argc, char **argv)
{
    gchar *name, *hostname = NULL;
    GOptionContext *context;
    GError *error = NULL;
    gchar ** args2[2] = { NULL, NULL };

    name = g_strdup_printf("- Simple VNC Client on Gtk-VNC %s",
                           vnc_util_get_version_string());
    /* Setup command line options */
    context = g_option_context_new (name);
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_add_group (context, vnc_display_get_option_group ());
    g_option_context_parse (context, &argc, &argv, &error);
    if (error) {
        g_print ("%s\n", error->message);
        g_error_free (error);
        return 1;
    }
    if (!args || (g_strv_length(args) != 1)) {
	    if((hostname = _vncviewer_get_hostname()) == NULL)
		    return 0;
	    args2[0] = hostname;
	    args = args2;
    }
    vncviewer(args);
    gtk_main();
    g_free(hostname);

    return 0;
}
