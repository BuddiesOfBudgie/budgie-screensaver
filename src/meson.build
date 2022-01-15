add_global_arguments([
	'-I' + meson.current_build_dir(),
	'-Db_sanitize=address'
], language: 'c')

gnome = import('gnome')
screensaver_marshal_files = gnome.genmarshal('gs-marshal', sources: 'gs-marshal.list', prefix: 'gs_marshal')

# Sources

screensaver_command_sources = [
    'gnome-screensaver-command.c',
]

screensaver_dialog_sources = [
    'gnome-screensaver-dialog.c',
    'gs-auth-pam.c',
    'gs-lock-plug.c',
    'gs-debug.c',
    'setuid.c',
    'subprocs.c',
]

screensaver_sources = [
    'gnome-screensaver.c',
	'gs-monitor.c',
	'gs-watcher-x11.c',
	'gs-listener-dbus.c',
	'gs-manager.c',
	'gs-window-x11.c',
	'gs-prefs.c',
	'gs-debug.c',
	'subprocs.c',
	'gs-grab-x11.c',
	'gs-fade.c',
]

# Dependencies

screensaver_command_deps = [dep_glib, dep_gio]

screensaver_dialog_deps = [dep_pam, dep_gthread, dep_gtk3, dep_gnomedesktop]

screensaver_deps = [dep_x11, dep_gtk3, dep_dbus, dep_gnomedesktop, dep_gsettings]

if with_systemd
    screensaver_deps += dep_systemd
endif

# Output

executable(
    'budgie-screensaver-command',
    sources: screensaver_command_sources,
    dependencies: screensaver_command_deps,
    include_directories: extra_includes,
    install: true
)

executable(
    'budgie-screensaver-dialog',
    sources: screensaver_dialog_sources,
    dependencies: screensaver_dialog_deps,
    include_directories: extra_includes,
    install: false
)

executable(
    'budgie-screensaver',
    sources: screensaver_sources + screensaver_marshal_files,
    dependencies: screensaver_deps,
    include_directories: extra_includes,
    install: true
)
