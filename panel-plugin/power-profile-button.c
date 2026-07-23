/*
 * xfce4-power-profile-plugin - Xfce panel plugin for power-profiles-daemon
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Panel button widget showing current power profile icon.
 * Left-click opens a popup menu to switch profiles.
 */

#include "power-profile-button.h"

#include <libxfce4util/libxfce4util.h>
#include <string.h>

/*  forward declarations  */
static void on_menu_deactivate (GtkMenu *menu, PowerProfileButton *button);

/*  Icon names for each profile */
#define ICON_PERFORMANCE  "power-profile-performance-symbolic"
#define ICON_BALANCED     "power-profile-balanced-symbolic"
#define ICON_POWER_SAVER  "power-profile-power-saver-symbolic"
#define ICON_ERROR        "power-profile-error-symbolic"

struct _PowerProfileButton
{
    GtkToggleButton  parent;
    PowerProfileDBus *dbus;
    GtkWidget        *image;
    GtkWidget        *menu;
};

G_DEFINE_TYPE (PowerProfileButton, xfpm_power_profile_button, GTK_TYPE_TOGGLE_BUTTON)

/*  helper: map profile name → icon name  */
static const gchar*
profile_to_icon (const gchar *profile)
{
    if (profile == NULL)
        return ICON_BALANCED;
    if (g_strcmp0 (profile, "performance") == 0)
        return ICON_PERFORMANCE;
    if (g_strcmp0 (profile, "power-saver") == 0)
        return ICON_POWER_SAVER;
    /* default to balanced for "balanced" and unknown profiles */
    return ICON_BALANCED;
}

/*  callback: menu item activated → set profile  */
static void
on_menu_item_activate (GtkMenuItem *mi, gpointer data)
{
    PowerProfileButton *btn = (PowerProfileButton *)data;
    const gchar *profile = g_object_get_data (G_OBJECT (mi), "profile-name");
    if (profile && btn->dbus)
        xfpm_power_profile_dbus_set_active (btn->dbus, profile);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (btn), FALSE);
}

/*  rebuild the popup menu from available profiles  */
static void
xfpm_power_profile_button_rebuild_menu (PowerProfileButton *button)
{
    GPtrArray *profiles;
    GSList *group = NULL;
    const gchar *active;
    GtkWidget *item;

    if (button->menu)
        gtk_widget_destroy (button->menu);

    button->menu = gtk_menu_new ();

    if (button->dbus == NULL || !xfpm_power_profile_dbus_is_available (button->dbus))
    {
        item = gtk_menu_item_new_with_label (_("Power Profiles not available"));
        gtk_widget_set_sensitive (item, FALSE);
        gtk_menu_shell_append (GTK_MENU_SHELL (button->menu), item);
        gtk_widget_show_all (button->menu);
        return;
    }

    profiles = xfpm_power_profile_dbus_get_profiles (button->dbus);
    active = xfpm_power_profile_dbus_get_active (button->dbus);

    for (guint i = 0; i < profiles->len; i++)
    {
        const gchar *name = g_ptr_array_index (profiles, i);
        gboolean is_active = (g_strcmp0 (name, active) == 0);

        item = gtk_radio_menu_item_new_with_label (group, name);
        group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

        if (is_active)
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);

        /* Store the profile name as action data */
        g_object_set_data_full (G_OBJECT (item), "profile-name",
                                g_strdup (name), g_free);

        g_signal_connect (item, "activate",
                          G_CALLBACK (on_menu_item_activate), button);

        gtk_menu_shell_append (GTK_MENU_SHELL (button->menu), item);
    }

    g_ptr_array_unref (profiles);

    /* Untoggle button when menu is dismissed */
    g_signal_connect (button->menu, "deactivate",
                      G_CALLBACK (on_menu_deactivate), button);

    gtk_widget_show_all (button->menu);
}

/*  update the icon + tooltip from current profile  */
static void
xfpm_power_profile_button_update (PowerProfileButton *button)
{
    const gchar *profile;
    const gchar *icon_name;
    gchar *tooltip;

    if (button->dbus == NULL || !xfpm_power_profile_dbus_is_available (button->dbus))
    {
        gtk_image_set_from_icon_name (GTK_IMAGE (button->image),
                                      ICON_ERROR, GTK_ICON_SIZE_MENU);
        gtk_widget_set_tooltip_text (GTK_WIDGET (button),
                                     _("Power Profiles daemon not available"));
        return;
    }

    profile = xfpm_power_profile_dbus_get_active (button->dbus);
    icon_name = profile_to_icon (profile);

    gtk_image_set_from_icon_name (GTK_IMAGE (button->image),
                                  icon_name, GTK_ICON_SIZE_MENU);

    tooltip = g_strdup_printf (_("Power Profile: %s"), profile);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip);
    g_free (tooltip);
}

/*  callback: D-Bus profile changed → update icon + rebuild menu  */
static void
on_profile_changed (PowerProfileDBus   *dbus,
                    const gchar        *profile,
                    PowerProfileButton *button)
{
    (void) dbus;
    (void) profile;
    xfpm_power_profile_button_update (button);
    xfpm_power_profile_button_rebuild_menu (button);
}

/*  callback: button click → show popup menu  */
static gboolean
on_button_press (GtkWidget      *widget,
                 GdkEventButton *event,
                 PowerProfileButton *button)
{
    if (event->button != GDK_BUTTON_PRIMARY)
        return FALSE;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
    xfpm_power_profile_button_rebuild_menu (button);

    gtk_menu_popup_at_widget (GTK_MENU (button->menu),
                              widget,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              (const GdkEvent *)event);

    return TRUE;
}

/*  when menu is dismissed, untoggle button  */
static void
on_menu_deactivate (GtkMenu           *menu,
                    PowerProfileButton *button)
{
    (void) menu;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
}

/*  ---------- GObject ----------  */

static void
xfpm_power_profile_button_init (PowerProfileButton *button)
{
    button->dbus = NULL;
    button->image = NULL;
    button->menu = NULL;
}

static void
xfpm_power_profile_button_finalize (GObject *object)
{
    PowerProfileButton *button = (PowerProfileButton *)object;
    if (button->menu)
        gtk_widget_destroy (button->menu);
    g_clear_object (&button->dbus);
    G_OBJECT_CLASS (xfpm_power_profile_button_parent_class)->finalize (object);
}

static void
xfpm_power_profile_button_class_init (PowerProfileButtonClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfpm_power_profile_button_finalize;
}

/*  ---------- public ----------  */

GtkWidget*
xfpm_power_profile_button_new (PowerProfileDBus *dbus)
{
    PowerProfileButton *button = g_object_new (XFPM_TYPE_POWER_PROFILE_BUTTON, NULL);

    if (dbus)
        button->dbus = g_object_ref (dbus);

    /* Create icon image */
    button->image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (button), button->image);

    /* Style as panel button (no relief, minimal padding) */
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

    /* Signals */
    g_signal_connect (button, "button-press-event",
                      G_CALLBACK (on_button_press), button);

    /* Connect to D-Bus changes */
    if (dbus)
        g_signal_connect (dbus, "profile-changed",
                          G_CALLBACK (on_profile_changed), button);

    /* Initial update */
    xfpm_power_profile_button_update (button);

    return GTK_WIDGET (button);
}
