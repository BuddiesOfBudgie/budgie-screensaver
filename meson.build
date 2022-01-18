project(
    'budgie-screensaver',
    'c',
    version: '10.0',
    license: ['GPL-2.0'],
    default_options: [
        'c_std=c11',
        'warning_level=2',
    ],
)

git = find_program('git', required: false)

# Dependencies
dep_x11 = dependency('x11', version: '>= 1.0')

dep_glib = dependency('glib-2.0', version: '>= 2.25.6')
dep_gio = dependency('gio-2.0', version: '>= 2.25.6')
dep_gthread = dependency('gthread-2.0', version: '>= 2.25.6')
dep_dbus = dependency('dbus-glib-1', version: '>= 0.3.0')

dep_gtk3 = dependency('gtk+-3.0', version: '>= 2.99.3')
dep_gnomedesktop = dependency('gnome-desktop-3.0', version: '>= 3.1.91')
dep_gsettings = dependency('gsettings-desktop-schemas', version: '>= 3.1.91')

with_systemd = get_option('with-systemd')
if with_systemd
    dep_systemd = dependency('libsystemd', version: '>= 209')
endif

c = meson.get_compiler('c')
dep_pam = c.find_library('pam', required: true)

# Inspired by https://github.com/GNOME/recipes/blob/master/meson.build
package_version = meson.project_version()

if git.found()
    git_version = run_command('git', ['rev-parse', '--short', 'HEAD'], check: false)
    if git_version.returncode() == 0
        package_version += ' (git-'+git_version.stdout().strip()+')'
    endif
endif

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
sysconfdir = get_option('sysconfdir')
podir = join_paths(meson.source_root(), 'po')

# create config.h
cdata = configuration_data()

cdata.set_quoted('VERSION', package_version)
cdata.set_quoted('PAM_SERVICE_NAME', meson.project_name())
cdata.set_quoted('LIBEXECDIR', libexecdir)

if with_systemd
    cdata.set('WITH_SYSTEMD', 1)
endif

configure_file(
    output: 'config.h',
    configuration: cdata,
)

# pull in the config.h file
extra_includes = [
    include_directories('.'),
]

# Handle i18n on the .desktop file
# Originally stolem from TingPing:
# https://github.com/vinszent/gnome-twitch/blob/master/data/meson.build
# Later changed to use intltool as we use _Key style files
intltool = find_program('intltool-merge')

# translations first
subdir('po')

# then source
subdir('src')

# and finally data
subdir('data')