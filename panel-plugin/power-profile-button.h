#ifndef POWER_PROFILE_BUTTON_H
#define POWER_PROFILE_BUTTON_H

#include <gtk/gtk.h>
#include "power-profile-dbus.h"

G_BEGIN_DECLS

#define XFPM_TYPE_POWER_PROFILE_BUTTON (xfpm_power_profile_button_get_type ())
G_DECLARE_FINAL_TYPE (PowerProfileButton, xfpm_power_profile_button, XFPM, POWER_PROFILE_BUTTON, GtkToggleButton)

GtkWidget* xfpm_power_profile_button_new (PowerProfileDBus *dbus);

G_END_DECLS

#endif /* POWER_PROFILE_BUTTON_H */
