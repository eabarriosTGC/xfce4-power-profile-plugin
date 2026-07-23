#ifndef POWER_PROFILE_DBUS_H
#define POWER_PROFILE_DBUS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define XFPM_TYPE_POWER_PROFILE_DBUS (xfpm_power_profile_dbus_get_type ())
G_DECLARE_FINAL_TYPE (PowerProfileDBus, xfpm_power_profile_dbus, XFPM, POWER_PROFILE_DBUS, GObject)

PowerProfileDBus*  xfpm_power_profile_dbus_new           (void);
const gchar*       xfpm_power_profile_dbus_get_active     (PowerProfileDBus *dbus);
void               xfpm_power_profile_dbus_set_active     (PowerProfileDBus *dbus, const gchar *profile);
GPtrArray*         xfpm_power_profile_dbus_get_profiles   (PowerProfileDBus *dbus);
gboolean           xfpm_power_profile_dbus_is_available   (PowerProfileDBus *dbus);

G_END_DECLS

#endif /* POWER_PROFILE_DBUS_H */
