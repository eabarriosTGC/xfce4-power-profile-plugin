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

/*  Icon names for each profile */
#define ICON_PERFORMANCE  "power-profile-performance-symbolic"
#define ICON_BALANCED     "power-profile-balanced-symbolic"
#define ICON_POWER_SAVER  "power-profile-power-saver-symbolic"
#define ICON_ERROR        "power-profile-error-symbolic"

struct _PowerProfileButton
{
    GtkButton        parent;
    PowerProfileDBus *dbus;
    GtkWidget        *image;
    GtkWidget        *menu;
    GPtrArray        *profile_names;  /* cache of profile name strings */
};

G_DEFINE_TYPE (PowerProfileButton, xfpm_power_profile_button, GTK_TYPE_BUTTON)

static const gchar*
profile_to_icon (const gchar *profile)
{
    if (profile == NULL)
        return ICON_BALANCED;
    if (g_strcmp0 (profile, "performance") == 0)
        return ICON_PERFORMANCE;
    if (g_strcmp0 (profile, "power-saver") == 0)
        return ICON_POWER_SAVER;
    return ICON_BALANCED;
}

/*  callback: menu item activated → set profile via D-Bus  */
static void
on_menu_item_activate (GtkMenuItem *mi, gpointer data)
{
    PowerProfileButton *btn = (PowerProfileButton *)data;
    const gchar *profile = g_object_get_data (G_OBJECT (mi), "profile-name");

    if (!profile)
    {
        return;
    }
    if (!btn->dbus)
    {
        return;
    }

    xfpm_power_profile_dbus_set_active (btn->dbus, profile);
}

/*  rebuild the popup menu  */
static void
xfpm_power_profile_button_rebuild_menu (PowerProfileButton *button)
{
    GPtrArray *profiles;
    const gchar *active;
    GtkWidget *item;
    guint i;


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

    for (i = 0; i < profiles->len; i++)
    {
        const gchar *name = g_ptr_array_index (profiles, i);
        gboolean is_active = (g_strcmp0 (name, active) == 0);


        /* Use check menu item so we can show the active state */
        item = gtk_check_menu_item_new_with_label (name);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), is_active);

        /* Make uncheckable — only the active one shows check */
        gtk_check_menu_item_set_draw_as_radio (GTK_CHECK_MENU_ITEM (item), FALSE);

        /* Store the profile name */
        g_object_set_data_full (G_OBJECT (item), "profile-name",
                                g_strdup (name), g_free);

        g_signal_connect (item, "activate",
                          G_CALLBACK (on_menu_item_activate), button);

        gtk_menu_shell_append (GTK_MENU_SHELL (button->menu), item);
    }

    g_ptr_array_unref (profiles);
    gtk_widget_show_all (button->menu);
}

/*  update the icon + tooltip  */
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

/*  D-Bus profile changed  */
static void
on_profile_changed (PowerProfileDBus   *dbus,
                    const gchar        *profile,
                    PowerProfileButton *button)
{
    (void) dbus;
    (void) profile;
    xfpm_power_profile_button_update (button);
}

/*  button clicked → show popup menu  */
static void
on_button_clicked (GtkButton *btn, PowerProfileButton *button)
{
    xfpm_power_profile_button_rebuild_menu (button);
    gtk_menu_popup_at_widget (GTK_MENU (button->menu),
                              GTK_WIDGET (btn),
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              NULL);
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
    PowerProfileButton *button;

    button = g_object_new (XFPM_TYPE_POWER_PROFILE_BUTTON, NULL);

    if (dbus)
        button->dbus = g_object_ref (dbus);

    button->image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (button), button->image);

    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

    g_signal_connect (button, "clicked",
                      G_CALLBACK (on_button_clicked), button);

    if (dbus)
        g_signal_connect (dbus, "profile-changed",
                          G_CALLBACK (on_profile_changed), button);

    xfpm_power_profile_button_update (button);

    return GTK_WIDGET (button);
}
