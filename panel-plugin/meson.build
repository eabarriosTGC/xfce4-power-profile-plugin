plugin_name = 'xfce4-power-profile-plugin'
plugin_sources = [
  'power-profile-plugin.c',
  'power-profile-dbus.c',
  'power-profile-upower.c',
  'power-profile-battery.c',
  'power-profile-button.c',
  'power-profile-dialogs.c',
]

plugin_headers = [
  'power-profile-plugin.h',
  'power-profile-dbus.h',
  'power-profile-upower.h',
  'power-profile-battery.h',
  'power-profile-button.h',
  'power-profile-dialogs.h',
]

plugin_lib = shared_module(
  plugin_name,
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  dependencies: [
    glib,
    gtk,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    gio,
    gio_unix,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / 'xfce4' / 'panel' / 'plugins',
)

# .desktop file
desktop_in = configuration_data()
desktop_in.set('PLUGIN_NAME', plugin_name)
desktop_in.set('PLUGIN_VERSION', meson.project_version())

desktop_file = plugin_name + '.desktop'
desktop_in_file = configure_file(
  input: plugin_name + '.desktop.in.in',
  output: plugin_name + '.desktop.in',
  configuration: desktop_in,
)

i18n.merge_file(
  input: desktop_in_file,
  output: desktop_file,
  po_dir: '../po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / 'xfce4' / 'panel' / 'plugins',
)
