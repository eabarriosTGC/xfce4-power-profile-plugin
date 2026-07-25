/*
 * xfce4-power-profile-plugin - UPower "OnBattery" watcher
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Same pattern as power-profile-dbus.c, but watches
 * org.freedesktop.UPower's OnBattery property.
 */

#include "power-profile-upower.h"

#define UPOWER_BUS_NAME    "org.freedesktop.UPower"
#define UPOWER_OBJECT_PATH "/org/freedesktop/UPower"
#define UPOWER_INTERFACE   "org.freedesktop.UPower"
#define PROPERTIES_IFACE   "org.freedesktop.DBus.Properties"

struct _PowerProfileUPower
{
    GObject     parent;
    GDBusProxy *proxy;
    gboolean    on_battery;
};

enum
{
    SIGNAL_ON_BATTERY_CHANGED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (PowerProfileUPower, xfpm_power_profile_upower, G_TYPE_OBJECT)

static void
xfpm_power_profile_upower_update (PowerProfileUPower *upower)
{
    GVariant *result;
    GVariant *value;

    result = g_dbus_proxy_call_sync (
        upower->proxy, "org.freedesktop.DBus.Properties.Get",
        g_variant_new ("(ss)", UPOWER_INTERFACE, "OnBattery"),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL
    );

    if (result == NULL)
        return;

    value = g_variant_get_variant (g_variant_get_child_value (result, 0));
    upower->on_battery = g_variant_get_boolean (value);

    g_variant_unref (value);
    g_variant_unref (result);
}

static void
on_properties_changed (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated,
                       gpointer    user_data)
{
    PowerProfileUPower *upower = XFPM_POWER_PROFILE_UPOWER (user_data);
    GVariant *battery_var;
    (void) proxy;
    (void) invalidated;

    battery_var = g_variant_lookup_value (changed_properties, "OnBattery",
                                          G_VARIANT_TYPE_BOOLEAN);
    if (battery_var == NULL)
        return;

    upower->on_battery = g_variant_get_boolean (battery_var);
    g_variant_unref (battery_var);

    g_signal_emit (upower, signals[SIGNAL_ON_BATTERY_CHANGED], 0, upower->on_battery);
}

static void
xfpm_power_profile_upower_init (PowerProfileUPower *upower)
{
    upower->proxy = NULL;
    upower->on_battery = FALSE;
}

static void
xfpm_power_profile_upower_finalize (GObject *object)
{
    PowerProfileUPower *upower = XFPM_POWER_PROFILE_UPOWER (object);
    g_clear_object (&upower->proxy);
    G_OBJECT_CLASS (xfpm_power_profile_upower_parent_class)->finalize (object);
}

static void
xfpm_power_profile_upower_class_init (PowerProfileUPowerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfpm_power_profile_upower_finalize;

    signals[SIGNAL_ON_BATTERY_CHANGED] = g_signal_new (
        "on-battery-changed",
        XFPM_TYPE_POWER_PROFILE_UPOWER,
        G_SIGNAL_RUN_FIRST,
        0, NULL, NULL,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE, 1, G_TYPE_BOOLEAN
    );
}

PowerProfileUPower*
xfpm_power_profile_upower_new (void)
{
    PowerProfileUPower *upower;
    GDBusProxy *proxy;
    GError *error = NULL;

    proxy = g_dbus_proxy_new_for_bus_sync (
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        UPOWER_BUS_NAME,
        UPOWER_OBJECT_PATH,
        PROPERTIES_IFACE,
        NULL,
        &error
    );

    if (proxy == NULL)
    {
        g_warning ("power-profile-plugin: UPower not available: %s",
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        return NULL;
    }

    upower = g_object_new (XFPM_TYPE_POWER_PROFILE_UPOWER, NULL);
    upower->proxy = proxy;

    xfpm_power_profile_upower_update (upower);

    g_signal_connect (proxy, "g-properties-changed",
                      G_CALLBACK (on_properties_changed), upower);

    return upower;
}

gboolean
xfpm_power_profile_upower_get_on_battery (PowerProfileUPower *upower)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_UPOWER (upower), FALSE);
    return upower->on_battery;
}

gboolean
xfpm_power_profile_upower_is_available (PowerProfileUPower *upower)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_UPOWER (upower), FALSE);
    return upower->proxy != NULL;
}
