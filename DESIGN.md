# Design: xfce4-power-profile-plugin

## Technical Approach

Native Xfce panel plugin (C + GTK3 + libxfce4panel-2.0) that wraps `power-profiles-daemon` D-Bus API (`net.hadess.PowerProfiles`). A button in the panel shows the current profile icon, and clicking opens a popup menu to switch profiles.

## Architecture Decisions

| Option | Tradeoffs | Decision |
|--------|-----------|----------|
| **Plugin API** | `libxfce4panel-2.0` (GObject) vs genmon script | Native: proper menu, D-Bus signals, theming |
| **D-Bus comms** | GDBus synchronous vs async | **Async** — `g_dbus_proxy_new_sync` for init, `g_signal_connect` for `PropertiesChanged` |
| **Icons** | Adwaita symbolic icons `power-profile-*` vs freedesktop | **Adwaita** — ships with GNOME icon theme; fallback to custom SVGs in our `icons/` |
| **Config** | xfconf (Xfce standard) vs flat file | **xfconf** — consistent with rest of Xfce |
| **Build** | Autotools vs Meson | **Meson** — all modern Xfce plugins use it since 4.18 |
| **PPD not running** | Crash vs degrade | **Degrade** — show error icon + "PPD not available" tooltip |

## Data Flow

```
┌─────────────────────────────────────────────────────┐
│                   Xfce Panel                        │
│  ┌──────────────────────────────────────────────┐   │
│  │  xfce4-power-profile-plugin                  │   │
│  │  ┌─────────────────┐  ┌──────────────────┐   │   │
│  │  │ PowerProfileBtn  │  │ PowerProfileMenu │   │   │
│  │  │  - GtkImage      │──│  - performance   │   │   │
│  │  │  - current icon  │  │  - balanced      │   │   │
│  │  └────────┬────────┘  │  - power-saver    │   │   │
│  │           │           └──────────────────┘   │   │
│  │           │ GObject signal                    │   │
│  │  ┌────────▼──────────────────────────────┐   │   │
│  │  │ PowerProfileDBus                       │   │   │
│  │  │  - GDBusProxy → net.hadess.PowerProfiles │   │
│  │  │  - PropertiesChanged handler           │   │   │
│  │  └───────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
         │ D-Bus system bus
         ▼
┌─────────────────────┐
│ power-profiles-daemon│
│  - performance       │
│  - balanced (✓)      │
│  - power-saver       │
└─────────────────────┘
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `meson.build` | Create | Top-level Meson build definition |
| `panel-plugin/meson.build` | Create | Subdir build: compiles the .so plugin |
| `panel-plugin/power-profile-plugin.c` | Create | Entry point: `XFCE_PANEL_PLUGIN_REGISTER`, construct/save/read |
| `panel-plugin/power-profile-plugin.h` | Create | `PowerProfilePlugin` GObject struct (extends `XfcePanelPlugin`) |
| `panel-plugin/power-profile-dbus.c` | Create | D-Bus proxy wrapper: init, get/set profile, signal handlers |
| `panel-plugin/power-profile-dbus.h` | Create | `PowerProfileDBus` interface declaration |
| `panel-plugin/power-profile-button.c` | Create | Button widget: icon display, click → popup menu |
| `panel-plugin/power-profile-button.h` | Create | Button widget declaration |
| `panel-plugin/power-profile-dialogs.c` | Create | Properties dialog (AC/battery default profiles) |
| `panel-plugin/power-profile-dialogs.h` | Create | Dialogs declarations |
| `panel-plugin/xfce4-power-profile.desktop.in.in` | Create | Plugin registration — `X-XFCE-Module=` + `Name=` |
| `icons/meson.build` | Create | Icon install definitions |
| `icons/power-profile-*-symbolic.svg` (4 files) | Create | Custom SVG icons for fallback |
| `README.md` | Create | Build/install instructions |
| `COPYING` | Create | GPL-2.0+ license text |

## Interfaces / Contracts

### D-Bus: `net.hadess.PowerProfiles` (system bus)

```c
// Properties we consume:
//   ActiveProfile (s)        — e.g. "balanced"
//   Profiles (aa{sv})        — available profiles with drivers
//   PerformanceInhibited (s) — e.g. "thermal"
//   PerformanceDegraded (s)  — e.g. "low-power"

// Method to change profile:
//   org.freedesktop.DBus.Properties.Set(
//       "net.hadess.PowerProfiles",
//       "ActiveProfile",
//       <"performance"|"balanced"|"power-saver">
//   )
```

### GObject Signal: `profile-changed`

```c
// Emitted by PowerProfileDBus when ActiveProfile changes
// g_signal_connect(dbus, "profile-changed", G_CALLBACK(handler), NULL);
```

### xfconf Properties

```
/xfce4-power-profile-plugin/profile-on-ac      → "balanced" (default)
/xfce4-power-profile-plugin/profile-on-battery  → "balanced" (default)
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Compile | Syntax, symbols, proper linking | `meson test` — at minimum build succeeds |
| Manual | UI, D-Bus integration | Install locally, test on real hardware |
| Future | Unit tests | `dbusmock` for power-profiles-daemon endpoint |

v1 focuses on **compile-time correctness** and **manual testing**. Automated testing via `dbusmock` is deferred.

## Migration / Rollout

No migration required — first release.

## Open Questions

- [ ] Icon theme: should we depend on `power-profile-*` from Adwaita or ship everything? (Decision: ship fallbacks, prefer system theme)
- [ ] Xfce 4.18 compat: `libxfce4panel-2.0` requires xfce4-panel >= 4.18. Should we also support 4.16? (Decision: target 4.18+, document requirement)
- [ ] Auto-switch AC/battery: useful but overlapping with xfpm. Offer in Properties but disabled by default?
