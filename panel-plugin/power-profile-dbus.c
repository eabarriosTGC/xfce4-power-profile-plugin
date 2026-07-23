/*
 * xfce4-power-profile-plugin - Xfce panel plugin for power-profiles-daemon
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * D-Bus wrapper for power-profiles-daemon (net.hadess.PowerProfiles).
 * Connects on the system bus, caches ActiveProfile, emits signals on changes.
 */

#include "power-profile-dbus.h"

#include <string.h>

/* D-Bus interface constants */
#define PPD_BUS_NAME      "net.hadess.PowerProfiles"
#define PPD_OBJECT_PATH   "/net/hadess/PowerProfiles"
#define PPD_INTERFACE     "net.hadess.PowerProfiles"
#define PROPERTIES_IFACE  "org.freedesktop.DBus.Properties"

struct _PowerProfileDBus
{
    GObject        parent;
    GDBusProxy    *proxy;
    gchar         *active_profile;
    GPtrArray     *profiles;     /* array of owned strings */
};

enum
{
    SIGNAL_PROFILE_CHANGED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (PowerProfileDBus, xfpm_power_profile_dbus, G_TYPE_OBJECT)

/* ---------- helpers ---------- */

static void
xfpm_power_profile_dbus_update_profiles (PowerProfileDBus *dbus)
{
    GVariant *result;
    GVariant *profiles_var;
    GVariantIter iter;
    GVariant *entry;

    if (dbus->profiles)
    {
        g_ptr_array_unref (dbus->profiles);
        dbus->profiles = NULL;
    }

    dbus->profiles = g_ptr_array_new_full (4, g_free);

    result = g_dbus_proxy_call_sync (
        dbus->proxy, "org.freedesktop.DBus.Properties.Get",
        g_variant_new ("(ss)", PPD_INTERFACE, "Profiles"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL
    );

    if (result == NULL)
        return;

    profiles_var = g_variant_get_variant (g_variant_get_child_value (result, 0));
    g_variant_iter_init (&iter, profiles_var);

    while ((entry = g_variant_iter_next_value (&iter)) != NULL)
    {
        const gchar *profile_name;
        g_variant_lookup (entry, "Profile", "&s", &profile_name);
        if (profile_name)
            g_ptr_array_add (dbus->profiles, g_strdup (profile_name));
        g_variant_unref (entry);
    }

    g_variant_unref (profiles_var);
    g_variant_unref (result);
}

static void
xfpm_power_profile_dbus_update_active (PowerProfileDBus *dbus)
{
    GVariant *result;
    GVariant *value;
    const gchar *active;

    result = g_dbus_proxy_call_sync (
        dbus->proxy, "org.freedesktop.DBus.Properties.Get",
        g_variant_new ("(ss)", PPD_INTERFACE, "ActiveProfile"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL
    );

    if (result == NULL)
        return;

    value = g_variant_get_variant (g_variant_get_child_value (result, 0));
    active = g_variant_get_string (value, NULL);

    g_free (dbus->active_profile);
    dbus->active_profile = g_strdup (active);

    g_variant_unref (value);
    g_variant_unref (result);
}

/* ---------- GObject machinery ---------- */

static void
xfpm_power_profile_dbus_init (PowerProfileDBus *dbus)
{
    dbus->proxy = NULL;
    dbus->active_profile = NULL;
    dbus->profiles = NULL;
}

static void
xfpm_power_profile_dbus_finalize (GObject *object)
{
    PowerProfileDBus *dbus = XFPM_POWER_PROFILE_DBUS (object);
    g_free (dbus->active_profile);
    if (dbus->profiles)
        g_ptr_array_unref (dbus->profiles);
    g_clear_object (&dbus->proxy);
    G_OBJECT_CLASS (xfpm_power_profile_dbus_parent_class)->finalize (object);
}

/* Callback: D-Bus PropertiesChanged signal */
static void
on_properties_changed (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated,
                       gpointer    user_data)
{
    PowerProfileDBus *dbus = XFPM_POWER_PROFILE_DBUS (user_data);
    GVariant *active_var;
    const gchar *new_profile;
    (void) proxy;
    (void) invalidated;

    active_var = g_variant_lookup_value (changed_properties, "ActiveProfile",
                                          G_VARIANT_TYPE_STRING);
    if (active_var == NULL)
        return;

    new_profile = g_variant_get_string (active_var, NULL);
    g_free (dbus->active_profile);
    dbus->active_profile = g_strdup (new_profile);
    g_variant_unref (active_var);

    g_signal_emit (dbus, signals[SIGNAL_PROFILE_CHANGED], 0, dbus->active_profile);
}

static void
xfpm_power_profile_dbus_class_init (PowerProfileDBusClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfpm_power_profile_dbus_finalize;

    signals[SIGNAL_PROFILE_CHANGED] = g_signal_new (
        "profile-changed",
        XFPM_TYPE_POWER_PROFILE_DBUS,
        G_SIGNAL_RUN_FIRST,
        0, NULL, NULL,
        g_cclosure_marshal_VOID__STRING,
        G_TYPE_NONE, 1, G_TYPE_STRING
    );
}

/* ---------- public API ---------- */

PowerProfileDBus*
xfpm_power_profile_dbus_new (void)
{
    PowerProfileDBus *dbus;
    GDBusProxy *proxy;
    GError *error = NULL;

    proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        PPD_BUS_NAME,
        PPD_OBJECT_PATH,
        PROPERTIES_IFACE,
        NULL,
        &error
    );

    if (proxy == NULL)
    {
        g_warning ("power-profiles-daemon not available on D-Bus: %s",
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        return NULL;
    }

    dbus = g_object_new (XFPM_TYPE_POWER_PROFILE_DBUS, NULL);
    dbus->proxy = proxy;

    /* Read initial state */
    xfpm_power_profile_dbus_update_active (dbus);
    xfpm_power_profile_dbus_update_profiles (dbus);

    /* Listen for changes */
    g_signal_connect (proxy, "g-properties-changed",
                      G_CALLBACK (on_properties_changed), dbus);

    return dbus;
}

const gchar*
xfpm_power_profile_dbus_get_active (PowerProfileDBus *dbus)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_DBUS (dbus), "balanced");
    return dbus->active_profile ? dbus->active_profile : "balanced";
}

void
xfpm_power_profile_dbus_set_active (PowerProfileDBus *dbus,
                                    const gchar      *profile)
{
    GVariant *result;
    GError *error = NULL;

    g_return_if_fail (XFPM_IS_POWER_PROFILE_DBUS (dbus));
    g_return_if_fail (profile != NULL);

    if (dbus->proxy == NULL)
        return;


    result = g_dbus_proxy_call_sync (
        dbus->proxy,
        "org.freedesktop.DBus.Properties.Set",
        g_variant_new ("(ssv)", PPD_INTERFACE, "ActiveProfile",
                       g_variant_new_string (profile)),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error != NULL)
    {
        g_error_free (error);
    }
    else
    {
        if (result != NULL)
            g_variant_unref (result);

        /* Update local cache immediately — don't wait for PropertiesChanged */
        g_free (dbus->active_profile);
        dbus->active_profile = g_strdup (profile);
        g_signal_emit (dbus, signals[SIGNAL_PROFILE_CHANGED], 0, profile);
    }
}

GPtrArray*
xfpm_power_profile_dbus_get_profiles (PowerProfileDBus *dbus)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_DBUS (dbus), NULL);

    if (dbus->profiles == NULL)
        return NULL;

    /* Return a copy so caller can iterate freely */
    GPtrArray *copy = g_ptr_array_new_full (dbus->profiles->len, g_free);
    for (guint i = 0; i < dbus->profiles->len; i++)
        g_ptr_array_add (copy, g_strdup (g_ptr_array_index (dbus->profiles, i)));
    return copy;
}

gboolean
xfpm_power_profile_dbus_is_available (PowerProfileDBus *dbus)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_DBUS (dbus), FALSE);
    return dbus->proxy != NULL;
}
