# xfce4-power-profile-plugin

**Xfce panel plugin** to view and switch power profiles (performance, balanced, power-saver) via `power-profiles-daemon`.

![Plugin in Xfce panel](screenshot.png)

## Features

- Shows current power profile icon in the panel
- Left-click opens a menu to switch between available profiles
- Icon updates in real-time when D-Bus signals a change
- Handles degraded/inhibited states gracefully
- Properties dialog with AC/battery profile defaults (reference only — **not** auto-switch)
- Degrades gracefully when `power-profiles-daemon` is not running

## Requirements

- **Xfce 4.18+** (panel with `libxfce4panel-2.0`)
- **power-profiles-daemon** running on D-Bus (system bus)
- GTK 3.24+, GLib 2.66+

## Build & Install

```bash
meson setup build
ninja -C build
sudo ninja -C build install
```

Restart the panel:

```bash
xfce4-panel -r
```

Then **right-click the panel → Add New Items → Power Profile Indicator**.

### Uninstall

```bash
sudo ninja -C build uninstall
```

## Usage

| Action | Result |
|--------|--------|
| Left-click button | Opens menu with available profiles |
| Select profile | Changes via D-Bus immediately |
| Hover | Tooltip shows current profile name |
| Right-click → Properties | Set reference AC/battery defaults |
| Right-click → Remove | Remove from panel |

## How it works

The plugin connects to `net.hadess.PowerProfiles` on the system D-Bus, monitors `ActiveProfile` via `PropertiesChanged` signals, and updates the icon + menu in real time. It uses `libxfce4panel-2.0` for native Xfce panel integration (no tray icons, no libappindicator).

Auto-switching between AC/battery profiles is intentionally left to **xfce4-power-manager** to avoid conflicting profile changes.

## Files

```
panel-plugin/
├── power-profile-plugin.c      # Plugin entry point
├── power-profile-plugin.h
├── power-profile-dbus.c         # D-Bus wrapper for PPD
├── power-profile-dbus.h
├── power-profile-button.c       # Panel button + popup menu
├── power-profile-button.h
├── power-profile-dialogs.c      # Properties dialog
├── power-profile-dialogs.h
└── xfce4-power-profile-plugin.desktop.in.in
icons/                           # Symbolic SVGs for each profile
meson.build                      # Top-level Meson build
```

## License

GNU General Public License v2.0 or later. See [COPYING](COPYING).

## Author

[LinuxeroGuajiro](https://github.com/eabarriosTGC) — Xfce power profile enthusiast.
