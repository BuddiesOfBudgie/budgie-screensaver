custom_target(
    'desktop-file-screensaver',
    input : 'budgie-screensaver.desktop.in',
    output : 'budgie-screensaver.desktop',
    command : [intltool, '--desktop-style', podir, '@INPUT@', '@OUTPUT@'],
    install_dir : join_paths(datadir, 'applications'),
    install : true,
)

install_data(
    'budgie-screensaver',
    install_dir: join_paths(sysconfdir, 'pam.d'),
)

install_man('budgie-screensaver-command.1')
install_man('budgie-screensaver.1')
