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


/* main */
int main(int argc, char **argv)
{
    gchar *name;
    GOptionContext *context;
    GError *error = NULL;

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
        fprintf(stderr, "Usage: " PROGNAME " [hostname][:display]\n");
        return 1;
    }
    vncviewer(args);
    gtk_main();

    return 0;
}
