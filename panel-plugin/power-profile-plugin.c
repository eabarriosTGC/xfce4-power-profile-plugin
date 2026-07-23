/*
 * xfce4-power-profile-plugin - Xfce panel plugin for power-profiles-daemon
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Plugin entry point. Registers with XfcePanel, wires D-Bus → button → menu.
 */

#include "power-profile-plugin.h"
#include "power-profile-dbus.h"
#include "power-profile-button.h"
#include "power-profile-dialogs.h"

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

struct _XfpmPowerProfilePlugin
{
    XfcePanelPlugin  parent;
    PowerProfileDBus *dbus;
    GtkWidget        *button;
};

G_DEFINE_TYPE (XfpmPowerProfilePlugin, xfpm_power_profile_plugin, XFCE_TYPE_PANEL_PLUGIN)

/*  ---------- callbacks ----------  */

static void
xfpm_power_profile_plugin_save (XfcePanelPlugin *panel_plugin)
{
    XfceRc *rc;
    gchar *file;

    file = xfce_panel_plugin_save_location (panel_plugin, TRUE);
    if (file == NULL)
        return;

    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);

    if (rc == NULL)
        return;

    /* Save orientation preference */
    xfce_rc_write_int_entry (rc, "orientation",
                              xfce_panel_plugin_get_orientation (panel_plugin));

    xfce_rc_close (rc);
}

static void
xfpm_power_profile_plugin_configure_plugin (XfcePanelPlugin *panel_plugin)
{
    xfpm_power_profile_dialogs_show (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel_plugin))));
}

static void
xfpm_power_profile_plugin_finalize (GObject *object)
{
    XfpmPowerProfilePlugin *plugin = XFPM_POWER_PROFILE_PLUGIN (object);
    g_clear_object (&plugin->dbus);
    G_OBJECT_CLASS (xfpm_power_profile_plugin_parent_class)->finalize (object);
}

/*  ---------- GObject ----------  */

static void
xfpm_power_profile_plugin_init (XfpmPowerProfilePlugin *plugin)
{
    plugin->dbus = NULL;
    plugin->button = NULL;
}

static void
xfpm_power_profile_plugin_class_init (XfpmPowerProfilePluginClass *klass)
{
    GObjectClass        *gobject_class = G_OBJECT_CLASS (klass);
    XfcePanelPluginClass *panel_class = XFCE_PANEL_PLUGIN_CLASS (klass);

    gobject_class->finalize = xfpm_power_profile_plugin_finalize;
    panel_class->save = xfpm_power_profile_plugin_save;
    panel_class->configure_plugin = xfpm_power_profile_plugin_configure_plugin;
}

/*  ---------- public ----------  */

static void
xfpm_power_profile_plugin_construct (XfcePanelPlugin *panel_plugin)
{
    XfpmPowerProfilePlugin *plugin = XFPM_POWER_PROFILE_PLUGIN (panel_plugin);

    /* Init xfconf (safe to call multiple times) */
    xfconf_init (NULL);

    /* Connect to power-profiles-daemon (may return NULL if not running) */
    plugin->dbus = xfpm_power_profile_dbus_new ();

    /* Create the button widget */
    plugin->button = xfpm_power_profile_button_new (plugin->dbus);

    /* Register as action widget so the panel knows this is interactive */
    xfce_panel_plugin_add_action_widget (panel_plugin, plugin->button);

    /* Add to panel */
    gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
    gtk_widget_show_all (GTK_WIDGET (plugin));

    /* Enable configure menu item */
    xfce_panel_plugin_menu_show_configure (panel_plugin);
}

XFCE_PANEL_PLUGIN_REGISTER (xfpm_power_profile_plugin_construct);
