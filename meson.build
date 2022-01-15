project(
    'budgie-screensaver',
    'c',
    version: '10.0',
    license: ['GPL-2.0'],
    default_options: [
        'c_std=gnu11',
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

# create config.h
cdata = configuration_data()

cdata.set_quoted('VERSION', package_version)
cdata.set_quoted('PAM_SERVICE_NAME', meson.project_name())

prefix = get_option('prefix')
libexecdir = join_paths(prefix, get_option('libexecdir'))

cdata.set_quoted('LIBEXECDIR', libexecdir)

configure_file(
    output: 'config.h',
    configuration: cdata,
)

extra_includes = [
    include_directories('.'),
]

# Handle i18n on the .desktop file
# Originally stolem from TingPing:
# https://github.com/vinszent/gnome-twitch/blob/master/data/meson.build
# Later changed to use intltool as we use _Key style files
intltool = find_program('intltool-merge')

subdir('src')
