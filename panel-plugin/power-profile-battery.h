/*
 * xfce4-power-profile-plugin - EMA-smoothed battery time estimator
 * Copyright (C) 2026  LinuxeroGuajiro
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Reads power_now from sysfs and applies an exponential moving average
 * (EMA) so the tooltip doesn't jump on every CPU/GPU load spike.
 * RAPL is used as a sanity signal to reject glitchy power_now readings.
 */

#ifndef POWER_PROFILE_BATTERY_H
#define POWER_PROFILE_BATTERY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFPM_TYPE_POWER_PROFILE_BATTERY (xfpm_power_profile_battery_get_type ())
G_DECLARE_FINAL_TYPE (PowerProfileBattery, xfpm_power_profile_battery, XFPM, POWER_PROFILE_BATTERY, GObject)

PowerProfileBattery* xfpm_power_profile_battery_new (void);

/* Seconds until empty, smoothed with EMA. -1 if no reliable data yet. */
gdouble  xfpm_power_profile_battery_get_time_to_empty (PowerProfileBattery *battery);
gboolean xfpm_power_profile_battery_is_available      (PowerProfileBattery *battery);

G_END_DECLS

#endif /* POWER_PROFILE_BATTERY_H */
