# Tasks: xfce4-power-profile-plugin v1

## Prereq: Install meson

```bash
sudo pacman -S meson
```

## Task list (8 tasks, implement in order)

### T1 — Project scaffold

**Files to create:**
- `meson.build` — top-level project definition
- `panel-plugin/meson.build` — shared library build (`.so` plugin)
- `icons/meson.build` — svg icon install
- `panel-plugin/xfce4-power-profile.desktop.in.in` — plugin registration

**Details:**
- Project name: `xfce4-power-profile-plugin`
- Plugin lib name: `libxfce4-power-profile-plugin.so`
- Dependencies: `glib-2.0`, `gtk+-3.0`, `libxfce4panel-2.0`, `libxfce4ui-2`, `libxfce4util-1.0`, `gio-2.0`, `libxfconf-0`
- Desktop file: `X-XFCE-Module=power-profile`, `Type=X-XFCE-PanelPlugin`, `Icon=power-profile-balanced-symbolic`
- C standard: gnu11
- License: GPL-2.0+
- Xfce version target: >= 4.18.0

**Verification:** `meson setup build` compiles (links nothing yet, just validates build system).

### T2 — D-Bus wrapper (power-profile-dbus.c/h)

**Files:**
- `panel-plugin/power-profile-dbus.h` — `GObject` declaration with signals
- `panel-plugin/power-profile-dbus.c` — implementation

**API:**
```c
// Sync init — return NULL if PPD not available
PowerProfileDBus*
power_profile_dbus_new (void);

// Get current state
const gchar*
power_profile_dbus_get_active_profile (PowerProfileDBus *dbus);

// Set profile — async via DBus Properties.Set
void
power_profile_dbus_set_active_profile (PowerProfileDBus *dbus, const gchar *profile);

// Get available profiles list
GPtrArray*
power_profile_dbus_get_profiles (PowerProfileDBus *dbus);  // array of strings

// Signals:
// "profile-changed" (gchar *new_profile)
// "profiles-changed" (GPtrArray *profiles)
// "performance-inhibited-changed" (gchar *reason)
// "performance-degraded-changed" (gchar *reason)
```

**Implementation details:**
- Internal: `GDBusProxy *proxy` connected to `net.hadess.PowerProfiles` on system bus
- Init: `g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, ...)` — returns NULL if PPD not running
- Cache `ActiveProfile` property locally
- Connect to `g_signal_connect(proxy, "g-properties-changed", ...)` — parse `ActiveProfile`, emit signals
- `set_active_profile`: call `g_dbus_proxy_call` with `org.freedesktop.DBus.Properties.Set`
- Thread safety: all GDBus operations on main thread (default main context)

**Verification:** Write a small test that compiles, creates the D-Bus proxy, reads ActiveProfile, prints it. (Test file can be temporary.)

### T3 — Button widget (power-profile-button.c/h)

**Files:**
- `panel-plugin/power-profile-button.h`
- `panel-plugin/power-profile-button.c`

**API:**
```c
GtkWidget*
power_profile_button_new (PowerProfileDBus *dbus);

// Update icon based on profile name
void
power_profile_button_set_profile (GtkWidget *button, const gchar *profile);

// Set error state (PPD unavailable)
void
power_profile_button_set_error (GtkWidget *button);
```

**Implementation details:**
- Extends `GtkToggleButton` (or `GtkButton` + `GtkEventBox`)
- Contains a `GtkImage` for the icon
- Left-click: popup `GtkMenu` with radio items for each available profile
- Icon selection logic:
  - `performance` → `power-profile-performance-symbolic`
  - `balanced` → `power-profile-balanced-symbolic`
  - `power-saver` → `power-profile-power-saver-symbolic`
  - error/unknown → `power-profile-error-symbolic`
  - Fallback: always try `gtk_icon_theme_load_icon` first, then our bundled path
- Tooltip: "Power Profile: {profile_name}" + if degraded/inhibited, append reason
- Connect to D-Bus signals to update icon + menu in real-time
- Menu item activation → `power_profile_dbus_set_active_profile()`

### T4 — Plugin entry point (power-profile-plugin.c/h)

**Files:**
- `panel-plugin/power-profile-plugin.h` — `PowerProfilePlugin` struct
- `panel-plugin/power-profile-plugin.c` — `XFCE_PANEL_PLUGIN_REGISTER`

**Struct:**
```c
typedef struct {
    XfcePanelPlugin    parent;
    
    XfcePanelPluginButton *plugin_button;  // Xfce's wrapper for panel buttons
    
    PowerProfileDBus     *dbus;
    GtkWidget            *main_widget;      // our PowerProfileButton
    GtkWidget            *properties_dialog;
    
    XfcePanelPlugin      *plugin;           // convenience ref
} PowerProfilePlugin;
```

**Entry point:**
```c
XFCE_PANEL_PLUGIN_REGISTER(power_profile_plugin_construct);

static void
power_profile_plugin_construct(XfcePanelPlugin *plugin) {
    // 1. Create PowerProfilePlugin struct
    // 2. Init DBus connection
    // 3. Create PowerProfileButton
    // 4. Add to panel via xfce_panel_plugin_add_action_widget
    // 5. Connect "save" and "free-data" signals
    // 6. xfce_panel_plugin_menu_show_configure(plugin, TRUE)
}
```

**Configuration signals:**
- `save`: persist xfconf settings
- `free-data`: cleanup
- `configure-plugin`: open properties dialog

### T5 — Properties dialog (power-profile-dialogs.c/h)

**Files:**
- `panel-plugin/power-profile-dialogs.h`
- `panel-plugin/power-profile-dialogs.c`

**UI:**
- GtkDialog with title "Power Profile Plugin Settings"
- Two GtkComboBox options:
  1. "Profile when on AC": [performance, balanced*, power-saver]
  2. "Profile when on battery": [performance, balanced*, power-saver]
- Default: both "balanced"
- "Close" button
- Reads/writes xfconf channel `xfce4-power-profile-plugin`
  - Property: `/profile-on-ac` (string)
  - Property: `/profile-on-battery` (string)

**Note:** These are **informational/fallback only** — the plugin does NOT auto-switch. The values are stored for the user's reference or future integration.

### T6 — SVG Icons

**Files:** (in `icons/`)
- `power-profile-performance-symbolic.svg`
- `power-profile-balanced-symbolic.svg`
- `power-profile-power-saver-symbolic.svg`
- `power-profile-error-symbolic.svg`

**Style:** Symbolic monochrome (single color, 16x16 viewBox), consistent with Adwaita symbolic icon style. Use `currentColor` for fill.

**Icon metaphors:**
- Performance: rocket / lightning bolt / gauge needle high
- Balanced: scales / gauge mid / half circle
- Power-saver: leaf / battery low / gauge low
- Error: warning triangle / X mark

**Verification:** `gtk3-icon-browser` or visually check in the panel.

### T7 — README + docs

- `README.md`: What it is, requirements (Xfce 4.18+, power-profiles-daemon), build instructions, install, screenshot placeholder
- `COPYING`: GPL-2.0+ license text (download from gnu.org)

### T8 — Local build & install

```bash
meson setup build
ninja -C build
sudo ninja -C build install
```

Then restart the panel:
```bash
xfce4-panel -r
```

Add the plugin: right-click panel → Add New Items → "Power Profile Indicator".

## Dependency graph

```
T1 (scaffold)
 ├── T2 (dbus) ──→ T3 (button) ──→ T4 (plugin) ──→ T8 (install)
T5 (dialogs) ────────────────────────┘
T6 (icons) ───────────────────────────┘
T7 (readme) ──────────────────────────┘ (independent, any time)
```

T1 must go first. T2, T5, T6, T7 can be partially parallel. T3 depends on T2. T4 depends on T3 + T5 + T6.
