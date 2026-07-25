#ifndef POWER_PROFILE_UPOWER_H
#define POWER_PROFILE_UPOWER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define XFPM_TYPE_POWER_PROFILE_UPOWER (xfpm_power_profile_upower_get_type ())
G_DECLARE_FINAL_TYPE (PowerProfileUPower, xfpm_power_profile_upower, XFPM, POWER_PROFILE_UPOWER, GObject)

PowerProfileUPower* xfpm_power_profile_upower_new            (void);
gboolean            xfpm_power_profile_upower_get_on_battery (PowerProfileUPower *upower);
gboolean            xfpm_power_profile_upower_is_available   (PowerProfileUPower *upower);

G_END_DECLS

#endif /* POWER_PROFILE_UPOWER_H */
