/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gnome-screensaver.h"
#include "gs-monitor.h"
#include "gs-debug.h"

gboolean attempt_gjs_screensaver_kill (gpointer data) {
	(void) data; // Don't need the data

	char *argv[5];
	argv[0] =  "pkill";
	argv[1] = "-9";
	argv[2] = "-f";
	argv[3] = "'/usr/bin/gjs /usr/share/gnome-shell/org.gnome.ScreenSaver'";
	argv[4] = NULL;

	g_autoptr(GError) error = NULL;
	gboolean kill_ret = g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, &error);

	if (!kill_ret) {
		g_warning("Failed to kill gjs: %s", error->message);
	}

	return G_SOURCE_REMOVE;
}

void
gnome_screensaver_quit (void)
{
	gtk_main_quit ();
}

int
main (int    argc,
      char **argv)
{
	GSMonitor          *monitor;
	GError             *error = NULL;
	static gboolean     show_version = FALSE;
	static gboolean     no_daemon    = TRUE;
	static gboolean     debug        = FALSE;
	static GOptionEntry entries []   = {
		{ "version", 0, 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL },
		{ "no-daemon", 0, 0, G_OPTION_ARG_NONE, &no_daemon, N_("Don't become a daemon"), NULL },
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging code"), NULL },
		{ NULL }
	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
	textdomain (GETTEXT_PACKAGE);
#endif

	if (! gtk_init_with_args (&argc, &argv, NULL, entries, NULL, &error)) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		} else {
			g_warning ("Unable to initialize GTK+");
		}
		exit (1);
	}

	if (show_version) {
		g_print ("%s %s\n", argv [0], VERSION);
		exit (1);
	}

		gchar** env_vars = g_get_environ(); // Get our list of environment variables
		gchar* desktop = g_environ_getenv(env_vars, "XDG_CURRENT_DESKTOP"); // Get the current desktop value

		if (desktop != NULL) { // Got a value
			if (!g_str_has_prefix(desktop, "Budgie")) { // Does not start with Budgie
				g_message("Not running under Budgie, exiting.");
				exit(1);
			}
		}

		g_strfreev(env_vars); // Free our environment variables

	gs_debug_init (debug, FALSE);
	gs_debug ("initializing budgie-screensaver %s", VERSION);

	monitor = gs_monitor_new ();

	if (monitor == NULL) {
	    exit (1);
	}

		// While the logic might seem weird, this saves us from having to implement a reading of /proc looking for gjs running the org.gnome.ScreenSaver "service", killing it, then doing a defer and kill again in case it starts up again
		// It also saves us from having to add any exponential backoff or logic for stopping the timer after N attempts.
		// Mostly just a stopgap to deal with situations where gnome-session is changed to either autostart or not autostart dbus interfaces and weirdness where people use GDM with Budgie.
		if (g_find_program_in_path("pkill") != NULL) { // Have pkill
			attempt_gjs_screensaver_kill(NULL); // Attempt kill immediately
			g_timeout_add_seconds_full(G_PRIORITY_LOW, 5, attempt_gjs_screensaver_kill, NULL, NULL); // Basically have a defer after 5s to attempt another kill of GJS
			g_timeout_add_seconds_full(G_PRIORITY_LOW, 10, attempt_gjs_screensaver_kill, NULL, NULL); // Basically have a defer after 10s to attempt another kill of GJS
			g_timeout_add_seconds_full(G_PRIORITY_LOW, 30, attempt_gjs_screensaver_kill, NULL, NULL); // In the event gjs is still going after 30s, kill again
		}

	error = NULL;
	if (! gs_monitor_start (monitor, &error)) {
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		} else {
			g_warning ("Unable to start screensaver");
		}
		exit (1);
	}

	gtk_main ();

	g_object_unref (monitor);

	gs_debug ("budgie-screensaver finished");

	gs_debug_shutdown ();

	return 0;
}
