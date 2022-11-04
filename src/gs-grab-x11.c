/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
#include <unistd.h>
#include <string.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#ifdef HAVE_XF86MISCSETGRABKEYSSTATE
#include <X11/extensions/xf86misc.h>
#endif /* HAVE_XF86MISCSETGRABKEYSSTATE */

#include "gs-window.h"
#include "gs-grab.h"
#include "gs-debug.h"

static void gs_grab_class_init(GSGrabClass* klass);
static void gs_grab_init(GSGrab* grab);
static void gs_grab_finalize(GObject* object);

static gpointer grab_object = NULL;

struct _GSGrabPrivate {
	GDBusConnection* session_bus;

	GdkSeat* seat;
	guint seat_hide_cursor: 1;
	GdkWindow* seat_grab_window;

	GtkWidget* invisible;
};

G_DEFINE_TYPE_WITH_PRIVATE (GSGrab, gs_grab, G_TYPE_OBJECT)

static const char* grab_string(int status) {
	switch (status) {
		case GDK_GRAB_SUCCESS:
			return "GrabSuccess";
		case GDK_GRAB_ALREADY_GRABBED:
			return "AlreadyGrabbed";
		case GDK_GRAB_INVALID_TIME:
			return "GrabInvalidTime";
		case GDK_GRAB_NOT_VIEWABLE:
			return "GrabNotViewable";
		case GDK_GRAB_FROZEN:
			return "GrabFrozen";
		default: {
			static char foo[255];
			sprintf(foo, "unknown status: %d", status);
			return foo;
		}
	}
}

#ifdef HAVE_XF86MISCSETGRABKEYSSTATE
/* This function enables and disables the Ctrl-Alt-KP_star and
   Ctrl-Alt-KP_slash hot-keys, which (in XFree86 4.2) break any
   grabs and/or kill the grabbing client.  That would effectively
   unlock the screen, so we don't like that.

   The Ctrl-Alt-KP_star and Ctrl-Alt-KP_slash hot-keys only exist
   if AllowDeactivateGrabs and/or AllowClosedownGrabs are turned on
   in XF86Config.  I believe they are disabled by default.

   This does not affect any other keys (specifically Ctrl-Alt-BS or
   Ctrl-Alt-F1) but I wish it did.  Maybe it will someday.
 */
static void
xorg_lock_smasher_set_active (GSGrab  *grab,
				  gboolean active)
{
	int status, event, error;

	if (!XF86MiscQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &event, &error)) {
		gs_debug ("No XFree86-Misc extension present");
		return;
	}

	if (active) {
		gs_debug ("Enabling the x.org grab smasher");
	} else {
		gs_debug ("Disabling the x.org grab smasher");
	}

	gdk_error_trap_push ();

	status = XF86MiscSetGrabKeysState (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), active);

	gdk_display_sync (gdk_display_get_default ());
	error = gdk_error_trap_pop ();

	if (active && status == MiscExtGrabStateAlready) {
		/* shut up, consider this success */
		status = MiscExtGrabStateSuccess;
	}

	if (error == Success) {
		gs_debug ("XF86MiscSetGrabKeysState(%s) returned %s\n",
			  active ? "on" : "off",
			  (status == MiscExtGrabStateSuccess ? "MiscExtGrabStateSuccess" :
			   status == MiscExtGrabStateLocked  ? "MiscExtGrabStateLocked"  :
			   status == MiscExtGrabStateAlready ? "MiscExtGrabStateAlready" :
			   "unknown value"));
	} else {
		gs_debug ("XF86MiscSetGrabKeysState(%s) failed with error code %d\n",
			  active ? "on" : "off", error);
	}
}
#else
static void xorg_lock_smasher_set_active(GSGrab* grab, gboolean active) {
	(void) grab;
	(void) active;
}
#endif /* HAVE_XF86MISCSETGRABKEYSSTATE */

static void gs_grab_move_focus(GdkWindow* window) {
	Window focus = 0;
	int rev = 0;

	GdkDisplay* default_display = gdk_display_get_default();

	gdk_x11_display_error_trap_push(default_display);

	XGetInputFocus(GDK_DISPLAY_XDISPLAY (default_display), &focus, &rev);

	XSetInputFocus(GDK_DISPLAY_XDISPLAY (default_display), GDK_WINDOW_XID (window), RevertToNone, CurrentTime);

	gdk_x11_display_error_trap_pop_ignored(default_display);
}

static void gs_grab_seat_grab_prepare_cb(GdkSeat* seat, GdkWindow* window, gpointer user_data) {
	(void) seat;
	(void) user_data;

	gdk_window_show(window);
	gs_grab_move_focus(window);
}

static int gs_grab_seat_grab(GSGrab* grab, GdkWindow* window, gboolean hide_cursor) {
	g_return_val_if_fail(window != NULL, FALSE);

	GdkCursor* cursor = NULL;
	if (!hide_cursor) {
		cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_BLANK_CURSOR);
	}

	GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
	GdkGrabStatus status =
		gdk_seat_grab(seat, window, GDK_SEAT_CAPABILITY_ALL, FALSE, cursor, NULL, gs_grab_seat_grab_prepare_cb, NULL);

	if (status == GDK_GRAB_SUCCESS) {
		if (grab->priv->seat_grab_window != NULL) {
			g_object_remove_weak_pointer(
				G_OBJECT(grab->priv->seat_grab_window),
				(gpointer*) &grab->priv->seat_grab_window
			);
		}

		if (grab->priv->seat != NULL) {
			g_object_remove_weak_pointer(G_OBJECT(grab->priv->seat), (gpointer*) &grab->priv->seat);
		}

		grab->priv->seat = seat;
		grab->priv->seat_grab_window = window;
		grab->priv->seat_hide_cursor = hide_cursor;

		g_object_add_weak_pointer(G_OBJECT(grab->priv->seat_grab_window), (gpointer*) &grab->priv->seat_grab_window);
		g_object_add_weak_pointer(G_OBJECT(grab->priv->seat), (gpointer*) &grab->priv->seat);
	} else {
		gs_debug ("Couldn't grab capabilities on seat!  (%s)", grab_string(status));
	}

	return status;
}

void gs_grab_seat_reset(GSGrab* grab) {
	if (grab->priv->seat_grab_window != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(grab->priv->seat_grab_window), (gpointer*) &grab->priv->seat_grab_window);
	}

	if (grab->priv->seat != NULL) {
		g_object_remove_weak_pointer(G_OBJECT(grab->priv->seat), (gpointer*) &grab->priv->seat);
	}

	grab->priv->seat_grab_window = NULL;
	grab->priv->seat = NULL;
}

void gs_grab_seat_ungrab(GSGrab* grab) {
	gdk_seat_ungrab(grab->priv->seat);
	gs_grab_seat_reset(grab);
}

static gboolean gs_grab_move_grab(GSGrab* grab, GdkWindow* window, gboolean hide_cursor) {
	gboolean result;
	GdkWindow* old_window;
	gboolean old_hide_cursor;

	/* if the pointer is not grabbed and we have a
	   mouse_grab_window defined then we lost the grab */
	if (grab->priv->seat != NULL
		&& !gdk_display_device_is_grabbed(gdk_display_get_default(), gdk_seat_get_pointer(grab->priv->seat))) {
		gs_grab_seat_reset(grab);
	}

	if (grab->priv->seat_grab_window == window) {
		gs_debug ("Window %X is already grabbed, skipping", (guint32) GDK_WINDOW_XID(grab->priv->seat_grab_window));
		return TRUE;
	}

#if 0
	gs_debug ("Intentionally skipping move pointer grabs");
	/* FIXME: GTK doesn't like having the pointer grabbed */
	return TRUE;
#else
	if (grab->priv->seat_grab_window) {
		gs_debug (
			"Moving pointer grab from %X to %X",
			(guint32) GDK_WINDOW_XID(grab->priv->seat_grab_window),
			(guint32) GDK_WINDOW_XID(window)
		);
	} else {
		gs_debug ("Getting pointer grab on %X", (guint32) GDK_WINDOW_XID(window));
	}
#endif

	gs_debug ("*** doing X server grab");
	gdk_x11_grab_server();

	old_window = grab->priv->seat_grab_window;
	old_hide_cursor = grab->priv->seat_hide_cursor;

	if (old_window) {
		gs_grab_seat_ungrab(grab);
	}

	result = gs_grab_seat_grab(grab, window, hide_cursor);

	if (result != GDK_GRAB_SUCCESS) {
		struct timespec remaining, request = {0, 2.5e8};
		nanosleep(&request, &remaining); // wait for 250 ms
		result = gs_grab_seat_grab(grab, window, hide_cursor);
	}

	if ((result != GDK_GRAB_SUCCESS) && old_window) {
		gs_debug ("Could not grab mouse for new window.  Resuming previous grab.");
		gs_grab_seat_grab(grab, old_window, old_hide_cursor);
	}

	gs_debug ("*** releasing X server grab");
	gdk_x11_ungrab_server();
	gdk_display_flush(gdk_display_get_default());

	return (result == GDK_GRAB_SUCCESS);
}

void gs_grab_release(GSGrab* grab) {
	gs_debug ("Releasing all grabs");

	gs_grab_seat_ungrab(grab);

	/* FIXME: is it right to enable this ? */
	xorg_lock_smasher_set_active(grab, TRUE);

	gdk_display_sync(gdk_display_get_default());
	gdk_display_flush(gdk_display_get_default());
}

/* The GNOME 3 Shell holds an X grab when we're in the overview;
 * ask it to bounce out before we try locking the screen.
 */
static void request_shell_exit_overview(GSGrab* grab) {
	GDBusMessage* message;

	/* Shouldn't happen, but... */
	if (!grab->priv->session_bus) {
		return;
	}

	message = g_dbus_message_new_method_call(
		"org.gnome.Shell",
		"/org/gnome/Shell",
		"org.freedesktop.DBus.Properties", "Set"
	);
	g_dbus_message_set_body(
		message,
		g_variant_new(
			"(ssv)",
			"org.gnome.Shell",
			"OverviewActive",
			g_variant_new("b", FALSE)
		)
	);

	g_dbus_connection_send_message(grab->priv->session_bus, message, G_DBUS_SEND_MESSAGE_FLAGS_NONE, NULL, NULL);
	g_object_unref(message);
}

gboolean gs_grab_grab_window(GSGrab* grab, GdkWindow* window, gboolean hide_cursor) {
	GdkGrabStatus status;
	int i;
	int retries = 4;
	gboolean focus_fuckus = FALSE;

	/* First, have stuff we control in GNOME un-grab */
	request_shell_exit_overview(grab);

AGAIN:

	for (i = 0; i < retries; i++) {
		status = gs_grab_seat_grab(grab, window, hide_cursor);
		if (status == GDK_GRAB_SUCCESS) {
			break;
		}

		/* else, wait a second and try to grab again. */
		sleep(1);
	}

	if (status != GDK_GRAB_SUCCESS) {
		if (!focus_fuckus) {
			focus_fuckus = TRUE;
			gs_grab_move_focus(window);
			goto AGAIN;
		}
	}

	/* Grab is good, go ahead and blank.  */
	return TRUE;
}

/* this is used to grab the keyboard and mouse to the root */
gboolean gs_grab_grab_root(GSGrab* grab, gboolean hide_cursor) {
	GdkDisplay* display;
	GdkWindow* root;
	GdkScreen* screen;
	gboolean res;

	gs_debug ("Grabbing the root window");

	display = gdk_display_get_default();
	GdkSeat* seat = gdk_display_get_default_seat(display);
	GdkDevice* pointer = gdk_seat_get_pointer(seat);
	gdk_device_get_position(pointer, &screen, NULL, NULL);
	root = gdk_screen_get_root_window(screen);

	res = gs_grab_grab_window(grab, root, hide_cursor);

	return res;
}

/* this is used to grab the keyboard and mouse to an offscreen window */
gboolean gs_grab_grab_offscreen(GSGrab* grab, gboolean hide_cursor) {
	gboolean res;

	gs_debug ("Grabbing an offscreen window");

	res = gs_grab_grab_window(grab, gtk_widget_get_window(grab->priv->invisible), hide_cursor);

	return res;
}

/* This is similar to gs_grab_grab_window but doesn't fail */
void gs_grab_move_to_window(GSGrab* grab, GdkWindow* window, gboolean hide_cursor) {
	gboolean result = FALSE;

	g_return_if_fail (GS_IS_GRAB(grab));

	xorg_lock_smasher_set_active(grab, FALSE);

	do {
		result = gs_grab_move_grab(grab, window, hide_cursor);
		gdk_display_flush(gdk_display_get_default());
	} while (!result);
}

static void gs_grab_class_init(GSGrabClass* klass) {
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gs_grab_finalize;
}

static void gs_grab_init(GSGrab* grab) {
	grab->priv = gs_grab_get_instance_private(grab);

	grab->priv->session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

	grab->priv->seat_hide_cursor = FALSE;
	grab->priv->invisible = gtk_invisible_new();
	gtk_widget_show(grab->priv->invisible);
}

static void gs_grab_finalize(GObject* object) {
	GSGrab* grab;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GS_IS_GRAB(object));

	grab = GS_GRAB (object);

	g_object_unref(grab->priv->session_bus);

	g_return_if_fail (grab->priv != NULL);

	gtk_widget_destroy(grab->priv->invisible);

	G_OBJECT_CLASS (gs_grab_parent_class)->finalize(object);
}

GSGrab* gs_grab_new(void) {
	if (grab_object) {
		g_object_ref (grab_object);
	} else {
		grab_object = g_object_new(GS_TYPE_GRAB, NULL);
		g_object_add_weak_pointer(
			grab_object, (gpointer*) &grab_object
		);
	}

	return GS_GRAB (grab_object);
}
