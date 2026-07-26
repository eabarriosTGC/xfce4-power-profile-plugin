#ifndef POWER_PROFILE_PLUGIN_H
#define POWER_PROFILE_PLUGIN_H

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

#include "power-profile-dbus.h"
#include "power-profile-button.h"
#include "power-profile-battery.h"

G_BEGIN_DECLS

#define XFPM_TYPE_POWER_PROFILE_PLUGIN (xfpm_power_profile_plugin_get_type())
G_DECLARE_FINAL_TYPE(XfpmPowerProfilePlugin, xfpm_power_profile_plugin, XFPM, POWER_PROFILE_PLUGIN, XfcePanelPlugin)

G_END_DECLS

#endif /* POWER_PROFILE_PLUGIN_H */
