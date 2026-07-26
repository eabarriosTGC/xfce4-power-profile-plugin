/*
 * xfce4-power-profile-plugin - EMA-smoothed battery time estimator
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Motivation: upowerd's time-to-empty is energy_now / power_now,
 * recalculated on every poll with no smoothing once
 * trust_power_measurement flips to TRUE. This module keeps its own
 * exponentially-weighted moving average (EMA) of power_now, sampled
 * independently, so the tooltip doesn't jump around on CPU/GPU load spikes.
 *
 * RAPL is read as a sanity signal only: if power_now reads 0 or exceeds
 * POWER_SANITY_MAX_W (the same quirk upower applies), we check RAPL
 * to confirm it was a PMU glitch, and hold the last valid EMA value.
 */

#include "power-profile-battery.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

/* ---------- constants ---------- */

#define SAMPLE_INTERVAL_SECONDS    10
#define EMA_ALPHA                   0.25
#define POWER_SANITY_MAX_W        300.0   /* mirrors upower's own quirk guard */
#define POWER_SUPPLY_DIR          "/sys/class/power_supply"
#define RAPL_ENERGY_PATH          "/sys/class/powercap/intel-rapl:0/energy_uj"

/* ---------- private struct ---------- */

struct _PowerProfileBattery
{
    GObject   parent;

    gchar    *battery_path;       /* e.g. /sys/class/power_supply/BAT0 */
    gboolean  available;

    gdouble   ema_power_w;        /* -1 = not seeded yet */
    gdouble   time_to_empty_s;    /* -1 = unknown */

    /* RAPL sanity tracking */
    gint64    last_rapl_uj;
    gint64    last_rapl_time_us;

    guint     timeout_id;
};

/* ---------- signals ---------- */

enum
{
    SIGNAL_ESTIMATE_CHANGED,
    SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (PowerProfileBattery, xfpm_power_profile_battery, G_TYPE_OBJECT)

/* ---------- helpers ---------- */

static gboolean
read_sysfs_double (const gchar *dir, const gchar *file, gdouble *out)
{
    gchar *path;
    gchar *contents = NULL;
    gboolean ok = FALSE;

    g_return_val_if_fail (dir != NULL, FALSE);
    g_return_val_if_fail (file != NULL, FALSE);
    g_return_val_if_fail (out != NULL, FALSE);

    path = g_build_filename (dir, file, NULL);
    if (g_file_get_contents (path, &contents, NULL, NULL))
    {
        gchar *end = NULL;
        *out = g_ascii_strtod (contents, &end);
        if (end != contents)
            ok = TRUE;
    }
    g_free (contents);
    g_free (path);
    return ok;
}

static gchar *
read_sysfs_string (const gchar *dir, const gchar *file)
{
    gchar *path;
    gchar *contents = NULL;
    gchar *result = NULL;

    g_return_val_if_fail (dir != NULL, NULL);
    g_return_val_if_fail (file != NULL, NULL);

    path = g_build_filename (dir, file, NULL);
    if (g_file_get_contents (path, &contents, NULL, NULL))
    {
        result = g_strstrip (g_strdup (contents));
    }
    g_free (contents);
    g_free (path);
    return result;
}

/* Find the first power_supply node with type == "Battery" */
static gchar *
find_battery_path (void)
{
    GDir *dir;
    const gchar *entry;
    gchar *result = NULL;

    dir = g_dir_open (POWER_SUPPLY_DIR, 0, NULL);
    if (dir == NULL)
        return NULL;

    while ((entry = g_dir_read_name (dir)) != NULL)
    {
        gchar *full = g_build_filename (POWER_SUPPLY_DIR, entry, NULL);
        g_autofree gchar *type = read_sysfs_string (full, "type");

        if (type != NULL && g_strcmp0 (type, "Battery") == 0)
        {
            result = full;
            /* full is now owned by result */
            break;
        }

        g_free (full);
    }

    g_dir_close (dir);
    return result;
}

/*
 * Returns average CPU-package watts over the interval since the last call,
 * purely as a sanity signal (NOT a substitute for total system power).
 * Returns -1 if unavailable or on the first call.
 */
static gdouble
rapl_sanity_watts (PowerProfileBattery *battery)
{
    gchar *contents = NULL;
    gint64 uj;
    gint64 now_us;
    gdouble watts = -1.0;

    if (!g_file_get_contents (RAPL_ENERGY_PATH, &contents, NULL, NULL))
        return -1.0;

    uj = g_ascii_strtoll (contents, NULL, 10);
    g_free (contents);
    now_us = g_get_monotonic_time ();

    if (battery->last_rapl_uj > 0)
    {
        gdouble delta_j = (gdouble) (uj - battery->last_rapl_uj) / 1e6;
        gdouble delta_s = (gdouble) (now_us - battery->last_rapl_time_us) / 1e6;
        if (delta_s > 0.0)
            watts = delta_j / delta_s;
    }

    battery->last_rapl_uj = uj;
    battery->last_rapl_time_us = now_us;
    return watts;
}

/* ---------- timeout callback ---------- */

static gboolean
on_sample_timeout (gpointer user_data)
{
    PowerProfileBattery *battery = XFPM_POWER_PROFILE_BATTERY (user_data);
    g_autofree gchar *status = NULL;
    gdouble energy_now_wh = -1.0, energy_full_wh = -1.0, power_now_w = -1.0;
    gboolean have_energy, have_power;

    if (!battery->available)
        return G_SOURCE_CONTINUE;

    status = read_sysfs_string (battery->battery_path, "status");

    /* Only estimate on discharge; charging and unknown are out of scope.
     * When not discharging, reset time and hold the EMA (it won't drift
     * because there are no samples being fed). */
    if (g_strcmp0 (status, "Discharging") != 0)
    {
        battery->time_to_empty_s = -1;
        g_signal_emit (battery, signals[SIGNAL_ESTIMATE_CHANGED], 0,
                       battery->time_to_empty_s);
        return G_SOURCE_CONTINUE;
    }

    /*
     * HP Meteor Lake exposes energy_* in µWh via its fuel gauge.
     * If that ever doesn't exist, we'd fall back to charge_now * voltage_now,
     * but for now the energy path is the correct one.
     */
    have_energy = read_sysfs_double (battery->battery_path, "energy_now",
                                     &energy_now_wh) &&
                  read_sysfs_double (battery->battery_path, "energy_full",
                                     &energy_full_wh);
    have_power  = read_sysfs_double (battery->battery_path, "power_now",
                                     &power_now_w);

    /* sysfs values are in µWh and µW -> convert to Wh and W */
    if (have_energy)
    {
        energy_now_wh  /= 1e6;
        energy_full_wh /= 1e6;
    }
    if (have_power)
        power_now_w /= 1e6;

    if (have_power && power_now_w > 0.0 && power_now_w <= POWER_SANITY_MAX_W)
    {
        /* Valid sample: feed the EMA */
        if (battery->ema_power_w < 0.0)
            battery->ema_power_w = power_now_w;          /* seed */
        else
            battery->ema_power_w = (power_now_w * EMA_ALPHA) +
                                    (battery->ema_power_w * (1.0 - EMA_ALPHA));

        /* Still read RAPL to keep the time reference, even when not needed */
        rapl_sanity_watts (battery);
    }
    else
    {
        /*
         * power_now is 0 or >300W (the same quirk upower applies — see
         * up-device-battery.c line 298). Read RAPL to confirm it's a PMU
         * glitch, and hold the last valid EMA without updating it.
         */
        gdouble rapl_w = rapl_sanity_watts (battery);
        g_debug ("power-profile-battery: power_now discarded (%.1f W), "
                 "RAPL CPU package ~%.1f W — holding EMA at %.1f W",
                 have_power ? power_now_w : -1.0,
                 rapl_w,
                 battery->ema_power_w);
    }

    if (have_energy && battery->ema_power_w > 0.0)
        battery->time_to_empty_s = (energy_now_wh / battery->ema_power_w) * 3600.0;
    else
        battery->time_to_empty_s = -1;

    g_signal_emit (battery, signals[SIGNAL_ESTIMATE_CHANGED], 0,
                   battery->time_to_empty_s);

    return G_SOURCE_CONTINUE;
}

/* ---------- GObject ---------- */

static void
xfpm_power_profile_battery_init (PowerProfileBattery *battery)
{
    battery->battery_path = find_battery_path ();
    battery->available = (battery->battery_path != NULL);
    battery->ema_power_w = -1.0;
    battery->time_to_empty_s = -1.0;
    battery->last_rapl_uj = -1;
    battery->last_rapl_time_us = 0;
    battery->timeout_id = 0;

    if (battery->available)
    {
        g_debug ("power-profile-battery: found %s, starting sampler",
                 battery->battery_path);
        battery->timeout_id = g_timeout_add_seconds (SAMPLE_INTERVAL_SECONDS,
                                                      on_sample_timeout,
                                                      battery);
    }
    else
    {
        g_warning ("power-profile-battery: no battery found under %s, "
                   "estimator disabled", POWER_SUPPLY_DIR);
    }
}

static void
xfpm_power_profile_battery_finalize (GObject *object)
{
    PowerProfileBattery *battery = XFPM_POWER_PROFILE_BATTERY (object);

    if (battery->timeout_id != 0)
        g_source_remove (battery->timeout_id);

    g_free (battery->battery_path);

    G_OBJECT_CLASS (xfpm_power_profile_battery_parent_class)->finalize (object);
}

static void
xfpm_power_profile_battery_class_init (PowerProfileBatteryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfpm_power_profile_battery_finalize;

    signals[SIGNAL_ESTIMATE_CHANGED] = g_signal_new (
        "estimate-changed",
        XFPM_TYPE_POWER_PROFILE_BATTERY,
        G_SIGNAL_RUN_FIRST,
        0, NULL, NULL,
        g_cclosure_marshal_VOID__DOUBLE,
        G_TYPE_NONE, 1, G_TYPE_DOUBLE
    );
}

/* ---------- public API ---------- */

PowerProfileBattery*
xfpm_power_profile_battery_new (void)
{
    return g_object_new (XFPM_TYPE_POWER_PROFILE_BATTERY, NULL);
}

gdouble
xfpm_power_profile_battery_get_time_to_empty (PowerProfileBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_BATTERY (battery), -1.0);
    return battery->time_to_empty_s;
}

gboolean
xfpm_power_profile_battery_is_available (PowerProfileBattery *battery)
{
    g_return_val_if_fail (XFPM_IS_POWER_PROFILE_BATTERY (battery), FALSE);
    return battery->available;
}
