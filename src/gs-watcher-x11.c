/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2006 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008      Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#include "config.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <gdk/gdkx.h>

#include "gs-watcher.h"
#include "gs-marshal.h"
#include "gs-debug.h"

static void     gs_watcher_class_init (GSWatcherClass *klass);
static void     gs_watcher_init       (GSWatcher      *watcher);
static void     gs_watcher_finalize   (GObject        *object);

static gboolean watchdog_timer        (GSWatcher      *watcher);

struct _GSWatcherPrivate
{
	/* settings */
	guint           enabled : 1;
	guint           delta_notice_timeout;

	/* state */
	guint           active : 1;
	guint           idle : 1;
	guint           idle_notice : 1;

	guint           idle_id;
	char           *status_message;

	GDBusProxy     *presence_proxy;
	guint           watchdog_timer_id;
};

enum {
	PROP_0,
	PROP_STATUS_MESSAGE
};

enum {
	IDLE_CHANGED,
	IDLE_NOTICE_CHANGED,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (GSWatcher, gs_watcher, G_TYPE_OBJECT)

static void
remove_watchdog_timer (GSWatcher *watcher)
{
	if (watcher->priv->watchdog_timer_id != 0) {
		g_source_remove (watcher->priv->watchdog_timer_id);
		watcher->priv->watchdog_timer_id = 0;
	}
}

static void
add_watchdog_timer (GSWatcher *watcher,
		    glong      timeout)
{
	watcher->priv->watchdog_timer_id = g_timeout_add_seconds (timeout,
								  (GSourceFunc)watchdog_timer,
								  watcher);
}

static void
gs_watcher_get_property (GObject    *object,
			 guint       prop_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
	GSWatcher *self;

	self = GS_WATCHER (object);

	switch (prop_id) {
	case PROP_STATUS_MESSAGE:
		g_value_set_string (value, self->priv->status_message);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_status_text (GSWatcher  *watcher,
		 const char *text)
{
	g_free (watcher->priv->status_message);

	watcher->priv->status_message = g_strdup (text);
	g_object_notify (G_OBJECT (watcher), "status-message");
}

static void
gs_watcher_set_property (GObject          *object,
			 guint             prop_id,
			 const GValue     *value,
			 GParamSpec       *pspec)
{
	GSWatcher *self;

	self = GS_WATCHER (object);

	switch (prop_id) {
	case PROP_STATUS_MESSAGE:
		set_status_text (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gs_watcher_class_init (GSWatcherClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gs_watcher_finalize;
	object_class->get_property = gs_watcher_get_property;
	object_class->set_property = gs_watcher_set_property;

	g_object_class_install_property (object_class,
					 PROP_STATUS_MESSAGE,
					 g_param_spec_string ("status-message",
							      NULL,
							      NULL,
							      NULL,
							      G_PARAM_READWRITE));

	signals [IDLE_CHANGED] =
		g_signal_new ("idle-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GSWatcherClass, idle_changed),
			      NULL,
			      NULL,
			      gs_marshal_BOOLEAN__BOOLEAN,
			      G_TYPE_BOOLEAN,
			      1, G_TYPE_BOOLEAN);
	signals [IDLE_NOTICE_CHANGED] =
		g_signal_new ("idle-notice-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GSWatcherClass, idle_notice_changed),
			      NULL,
			      NULL,
			      gs_marshal_BOOLEAN__BOOLEAN,
			      G_TYPE_BOOLEAN,
			      1, G_TYPE_BOOLEAN);
}

static gboolean
_gs_watcher_set_session_idle_notice (GSWatcher *watcher,
				     gboolean   in_effect)
{
	gboolean res;

	res = FALSE;

	if (in_effect != watcher->priv->idle_notice) {

		g_signal_emit (watcher, signals [IDLE_NOTICE_CHANGED], 0, in_effect, &res);
		if (res) {
			gs_debug ("Changing idle notice state: %d", in_effect);

			watcher->priv->idle_notice = in_effect;
		} else {
			gs_debug ("Idle notice signal not handled: %d", in_effect);
		}
	}

	return res;
}

static gboolean
_gs_watcher_set_session_idle (GSWatcher *watcher,
			      gboolean   is_idle)
{
	gboolean res;

	res = FALSE;

	if (is_idle != watcher->priv->idle) {

		g_signal_emit (watcher, signals [IDLE_CHANGED], 0, is_idle, &res);
		if (res) {
			gs_debug ("Changing idle state: %d", is_idle);

			watcher->priv->idle = is_idle;
		} else {
			gs_debug ("Idle changed signal not handled: %d", is_idle);
		}
	}

	return res;
}

gboolean
gs_watcher_get_active (GSWatcher *watcher)
{
	gboolean active;

	g_return_val_if_fail (GS_IS_WATCHER (watcher), FALSE);

	active = watcher->priv->active;

	return active;
}

static void
_gs_watcher_reset_state (GSWatcher *watcher)
{
	watcher->priv->idle = FALSE;
	watcher->priv->idle_notice = FALSE;
}

static gboolean
_gs_watcher_set_active_internal (GSWatcher *watcher,
				 gboolean   active)
{
	if (active != watcher->priv->active) {
		/* reset state */
		_gs_watcher_reset_state (watcher);

		watcher->priv->active = active;
	}

	return TRUE;
}

gboolean
gs_watcher_set_active (GSWatcher *watcher,
		       gboolean   active)
{
	g_return_val_if_fail (GS_IS_WATCHER (watcher), FALSE);

	gs_debug ("turning watcher: %s", active ? "ON" : "OFF");

	if (watcher->priv->active == active) {
		gs_debug ("Idle detection is already %s",
			  active ? "active" : "inactive");
		return FALSE;
	}

	if (! watcher->priv->enabled) {
		gs_debug ("Idle detection is disabled, cannot activate");
		return FALSE;
	}

	return _gs_watcher_set_active_internal (watcher, active);
}

gboolean
gs_watcher_set_enabled (GSWatcher *watcher,
			gboolean   enabled)
{
	g_return_val_if_fail (GS_IS_WATCHER (watcher), FALSE);

	if (watcher->priv->enabled != enabled) {
		gboolean is_active = gs_watcher_get_active (watcher);

		watcher->priv->enabled = enabled;

		/* if we are disabling the watcher and we are
		   active shut it down */
		if (! enabled && is_active) {
			_gs_watcher_set_active_internal (watcher, FALSE);
		}
	}

	return TRUE;
}

gboolean
gs_watcher_get_enabled (GSWatcher *watcher)
{
	gboolean enabled;

	g_return_val_if_fail (GS_IS_WATCHER (watcher), FALSE);

	enabled = watcher->priv->enabled;

	return enabled;
}

static gboolean
on_idle_timeout (GSWatcher *watcher)
{
	gboolean res;

	res = _gs_watcher_set_session_idle (watcher, TRUE);

	_gs_watcher_set_session_idle_notice (watcher, FALSE);

	/* try again if we failed i guess */
	return !res;
}

static void
set_status (GSWatcher *watcher,
	    guint      status)
{
	gboolean is_idle;

	if (! watcher->priv->active) {
		gs_debug ("GSWatcher: not active, ignoring status changes");
		return;
	}

	is_idle = (status == 3);

	if (!is_idle && !watcher->priv->idle_notice) {
		/* no change in idleness */
		return;
	}

	if (is_idle) {
		_gs_watcher_set_session_idle_notice (watcher, is_idle);
		/* queue an activation */
		if (watcher->priv->idle_id > 0) {
			g_source_remove (watcher->priv->idle_id);
		}
		watcher->priv->idle_id = g_timeout_add_seconds (watcher->priv->delta_notice_timeout,
								(GSourceFunc)on_idle_timeout,
								watcher);
	} else {
		/* cancel notice too */
		if (watcher->priv->idle_id > 0) {
			g_source_remove (watcher->priv->idle_id);
		}
		_gs_watcher_set_session_idle (watcher, FALSE);
		_gs_watcher_set_session_idle_notice (watcher, FALSE);
	}
}

static void on_presence_signal (
	GDBusProxy* proxy,
	const char* sender,
	const char* signal,
	GVariant* parameters,
	void* user_data
) {
	(void) proxy;

	if (strcmp(sender, "org.gnome.SessionManager") != 0) return;

	GSWatcher* watcher = GS_WATCHER(user_data);
	if (strcmp(signal, "StatusChanged") == 0 && g_variant_is_of_type(parameters, G_VARIANT_TYPE("(u)"))) {
		uint32_t status;
		g_variant_get(parameters, "(u)", &status);
		set_status(watcher, status);
	} else if (strcmp(signal, "StatusTextChanged") == 0 && g_variant_is_of_type(parameters, G_VARIANT_TYPE("(s)"))) {
		const char *status_text;
		g_variant_get(parameters, "(s)", &status_text);
		set_status_text(watcher, status_text);
	}
}

static GVariant* get_presence_property(GDBusProxy* proxy, const char* property) {
	GError *err = NULL;

	GVariant* variant = g_dbus_proxy_call_sync(
		proxy,
		"Get",
		g_variant_new("(ss)", "org.gnome.SessionManager.Presence", property),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&err
	);

	if (err != NULL) {
		g_warning("Failed to get %s property for org.gnome.SessionManager.Presence: %s", property, err->message);
		g_error_free(err);
		g_object_unref(proxy);
	}

	return variant;
}

static bool connect_presence_watcher(GSWatcher* watcher) {
	GError *err = NULL;
	watcher->priv->presence_proxy = g_dbus_proxy_new_for_bus_sync(
		G_BUS_TYPE_SESSION,
		G_DBUS_PROXY_FLAGS_NONE,
		NULL,
		"org.gnome.SessionManager",
		"/org/gnome/SessionManager/Presence",
		"org.gnome.SessionManager.Presence",
		NULL,
		&err
	);

	if (err != NULL) {
		g_warning("Failed to get proxy for org.gnome.SessionManager: %s", err->message);
		g_error_free(err);
		return false;
	}

	g_signal_connect(watcher->priv->presence_proxy, "g-signal", G_CALLBACK(on_presence_signal), watcher);
	GDBusProxy* proxy = g_dbus_proxy_new_sync(
		g_dbus_proxy_get_connection(watcher->priv->presence_proxy),
		G_DBUS_PROXY_FLAGS_NONE,
		NULL,
		g_dbus_proxy_get_name(watcher->priv->presence_proxy),
		g_dbus_proxy_get_object_path(watcher->priv->presence_proxy),
		"org.freedesktop.DBus.Properties",
		NULL,
		&err
	);

	if (err != NULL) {
		g_warning("Failed to get properties object for org.gnome.SessionManager: %s", err->message);
		g_error_free(err);
		return false;
	}


	GVariant* variant = get_presence_property(proxy, "status");
	if (variant == NULL) return false;
	uint32_t status;
	g_variant_get(variant, "(u)", &status);
	g_variant_unref(variant);

	variant = get_presence_property(proxy, "status-text");
	if (variant == NULL) return false;
	char* status_text;
	g_variant_get(variant, "(s)", &status_text);
	g_variant_unref(variant);

	set_status(watcher, status);
	set_status_text(watcher, status_text);

	return true;
}

static void
gs_watcher_init (GSWatcher *watcher)
{
	watcher->priv = gs_watcher_get_instance_private (watcher);

	watcher->priv->enabled = TRUE;
	watcher->priv->active = FALSE;

	connect_presence_watcher (watcher);

	/* time before idle signal to send notice signal */
	watcher->priv->delta_notice_timeout = 10;

	add_watchdog_timer (watcher, 600);
}

static void
gs_watcher_finalize (GObject *object)
{
	GSWatcher *watcher;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GS_IS_WATCHER (object));

	watcher = GS_WATCHER (object);

	g_return_if_fail (watcher->priv != NULL);

	remove_watchdog_timer (watcher);

	if (watcher->priv->idle_id > 0) {
		g_source_remove (watcher->priv->idle_id);
	}

	watcher->priv->active = FALSE;

	if (watcher->priv->presence_proxy != NULL) {
		g_object_unref (watcher->priv->presence_proxy);
	}

	g_free (watcher->priv->status_message);

	G_OBJECT_CLASS (gs_watcher_parent_class)->finalize (object);
}

/* Figuring out what the appropriate XSetScreenSaver() parameters are
   (one wouldn't expect this to be rocket science.)
*/
static void
disable_builtin_screensaver (GSWatcher *watcher,
			     gboolean   unblank_screen)
{
	(void) watcher;

	int current_server_timeout, current_server_interval;
	int current_prefer_blank,   current_allow_exp;
	int desired_server_timeout, desired_server_interval;
	int desired_prefer_blank,   desired_allow_exp;

	XGetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
			 &current_server_timeout,
			 &current_server_interval,
			 &current_prefer_blank,
			 &current_allow_exp);

	desired_server_timeout  = current_server_timeout;
	desired_server_interval = current_server_interval;
	desired_prefer_blank    = current_prefer_blank;
	desired_allow_exp       = current_allow_exp;

	desired_server_interval = 0;

	/* I suspect (but am not sure) that DontAllowExposures might have
	   something to do with powering off the monitor as well, at least
	   on some systems that don't support XDPMS?  Who know... */
	desired_allow_exp = AllowExposures;

	/* When we're not using an extension, set the server-side timeout to 0,
	   so that the server never gets involved with screen blanking, and we
	   do it all ourselves.  (However, when we *are* using an extension,
	   we tell the server when to notify us, and rather than blanking the
	   screen, the server will send us an X event telling us to blank.)
	*/
	desired_server_timeout = 0;

	if (desired_server_timeout     != current_server_timeout
	    || desired_server_interval != current_server_interval
	    || desired_prefer_blank    != current_prefer_blank
	    || desired_allow_exp       != current_allow_exp) {

		gs_debug ("disabling server builtin screensaver:"
			  " (xset s %d %d; xset s %s; xset s %s)",
			  desired_server_timeout,
			  desired_server_interval,
			  (desired_prefer_blank ? "blank" : "noblank"),
			  (desired_allow_exp ? "expose" : "noexpose"));

		XSetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
				 desired_server_timeout,
				 desired_server_interval,
				 desired_prefer_blank,
				 desired_allow_exp);

		XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
	}

	if (unblank_screen) {
		/* Turn off the server builtin saver if it is now running. */
		XForceScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), ScreenSaverReset);
	}
}


/* This timer goes off every few minutes, whether the user is idle or not,
   to try and clean up anything that has gone wrong.

   It calls disable_builtin_screensaver() so that if xset has been used,
   or some other program (like xlock) has messed with the XSetScreenSaver()
   settings, they will be set back to sensible values (if a server extension
   is in use, messing with xlock can cause the screensaver to never get a wakeup
   event, and could cause monitor power-saving to occur, and all manner of
   heinousness.)

 */

static gboolean
watchdog_timer (GSWatcher *watcher)
{

	disable_builtin_screensaver (watcher, FALSE);

	return TRUE;
}

GSWatcher *
gs_watcher_new (void)
{
	GSWatcher *watcher;

	watcher = g_object_new (GS_TYPE_WATCHER,
				NULL);

	return GS_WATCHER (watcher);
}
