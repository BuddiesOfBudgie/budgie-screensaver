# Budgie Screensaver

[![GitHub release (latest by date)](https://img.shields.io/github/v/release/BuddiesOfBudgie/budgie-screensaver)](https://github.com/BuddiesOfBudgie/budgie-screensaver/releases)
[![Translate into your language!](https://img.shields.io/badge/help%20translate-Transifex-4AB)](https://www.transifex.com/buddiesofbudgie/budgie-screensaver)
[![Chat with us on Matrix](https://img.shields.io/badge/chat-on%20Matrix-%230098D4)](https://matrix.to/#/#buddies-of-budgie:matrix.org)

[![](https://opencollective.com/buddies-of-budgie/tiers/backer.svg?avatarHeight=96)](https://opencollective.com/buddies-of-budgie)

Budgie Screensaver is a fork of gnome-screensaver intended for use with Budgie Desktop and is similar in purpose to other screensavers such as MATE Screensaver. This repository and its respective software should largely be considered as being in "maintenance mode", only updating to resolve FTBFS issues or reflect changes in the GNOME Stack.

## History

Budgie Screensaver traces its origins back to the original [XScreenSaver](https://www.jwz.org/xscreensaver/) screen saver, written by Jamie Zawinski and others, originally published under the BSD license. Much of this code was copied verbatim into GNOME Screensaver with its attribution stripped, and then relicensed as GPL 2.0.

This fork was born out of a need in Budgie 10 series to have a drop-in replacement of gnome-screensaver for use in Budgie, after upstream [indicated](https://mail.gnome.org/archives/desktop-devel-list/2016-July/msg00030.html) the appropriate continued use of gnome-screensaver is a fork, as they are not planning on taking patches. The overwhelming majority of patches are ported from Debian and/or Ubuntu's package-fork of gnome-screensaver to allow a simple drop-in replacement for the gnome-screensaver package.

## Compiling

Budgie Screensaver makes use of Meson for configuration, compilation, and installation. As such, the standard method is to:

1. `meson build`
2. `meson compile -C build`
3. `sudo meson install -C build`

## LICENSE

Budgie Screensaver is licensed under GPL-2.0-only.
