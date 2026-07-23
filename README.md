# xfce4-power-profile-plugin

**Xfce panel plugin** to view and switch power profiles (performance, balanced, power-saver) via `power-profiles-daemon`.

![screenshot](screenshot.png)

## Features

- Shows current power profile icon in the panel (performance/balanced/power-saver)
- Left-click opens a menu to switch between available profiles
- Icon updates in real-time via D-Bus signals
- Properties dialog with AC/battery profile defaults (reference only)
- Graceful degradation when `power-profiles-daemon` is not running
- Native Xfce panel integration — no tray icons or libappindicator

## Requirements

| Dependency | Minimum | Notes |
|------------|---------|-------|
| **Xfce Panel** | 4.18 | `libxfce4panel-2.0` — ships with Xfce 4.18+ |
| **power-profiles-daemon** | 0.20 | Ships with GNOME, but works standalone |
| **GTK** | 3.24 | |
| **GLib** | 2.66 | |

### Distro compatibility

| Distro | Xfce version | Works? |
|--------|-------------|--------|
| **CachyOS** | 4.20 | ✅ Native |
| **Arch / Manjaro** | 4.18+ | ✅ `sudo pacman -S meson` |
| **Fedora 38+** | 4.18+ | ✅ `sudo dnf install meson` |
| **Xubuntu 22.04+** | 4.18+ | ✅ `sudo apt install meson` |
| **Debian 12+** | 4.18+ | ✅ `sudo apt install meson` |
| **Linux Mint Xfce** | 4.18+ | ✅ `sudo apt install meson` |
| **Xubuntu 20.04 / Debian 11** | 4.16 | ❌ No soportado (usa `libxfce4panel-1.0`) |

> Nota: `power-profiles-daemon` se instala por separado si no viene con tu distro:
> - Debian/Ubuntu: `sudo apt install power-profiles-daemon`
> - Fedora: `sudo dnf install power-profiles-daemon`
> - Arch: `sudo pacman -S power-profiles-daemon`

## Build & Install

```bash
git clone https://github.com/eabarriosTGC/xfce4-power-profile-plugin
cd xfce4-power-profile-plugin
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
| Left-click the icon | Opens menu with available profiles |
| Select a profile | Switches immediately via D-Bus |
| Hover | Tooltip shows current profile name |
| Right-click → Properties | Set AC/battery defaults (reference only) |
| Right-click → Remove | Remove from panel |

## How it works

The plugin connects to `net.hadess.PowerProfiles` on the system D-Bus bus, monitors `ActiveProfile` via `PropertiesChanged` signals, and updates the icon + menu in real time. Uses `libxfce4panel-2.0` for native Xfce panel integration.

Auto-switching between AC/battery profiles is intentionally left to **xfce4-power-manager** to avoid conflicting profile changes.

## Stress test results

CPU-bound workload (5 seconds, all cores) on an Intel Meteor Lake laptop:

```
Profile              Avg      Peak     Min      Max
─────────────────────────────────────────────────────
🌱  power-saver     1238     2003     400     2012   MHz
⚖️  balanced        1236     2034     400     2331   MHz
🚀  performance     1383     4117     400     4286   MHz

Performance avg is 12% higher than power-saver
Peak boost: 4117 MHz vs 2003 MHz (2× faster under sustained load)
```

The frequency governor stays on `powersave` on modern Intel CPUs — the actual control is through **EPP** (Energy Performance Preference), which changes with each profile:

- `power-saver` → EPP `power` (prioritizes battery life)
- `balanced` → EPP `balance_power` (default)
- `performance` → EPP `performance` (allows full turbo boost)

## Project structure

```
panel-plugin/
├── power-profile-plugin.c      # Entry point (XFCE_PANEL_PLUGIN_REGISTER)
├── power-profile-plugin.h
├── power-profile-dbus.c         # D-Bus wrapper for net.hadess.PowerProfiles
├── power-profile-dbus.h
├── power-profile-button.c       # Panel button + popup menu
├── power-profile-button.h
├── power-profile-dialogs.c      # Properties dialog (xfconf)
├── power-profile-dialogs.h
└── xfce4-power-profile-plugin.desktop.in.in
icons/                           # 4 symbolic SVGs
meson.build                      # Meson build system
```

## License

GNU General Public License v2.0 or later. See [COPYING](COPYING).

## Author

[LinuxeroGuajiro](https://github.com/eabarriosTGC)
