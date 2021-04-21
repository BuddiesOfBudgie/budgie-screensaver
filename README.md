# budgie-screensaver

Budgie Screensaver is a fork of gnome-screensaver intended for use with Budgie Desktop and is similar in purpose to other screensavers such as MATE Screensaver.

This fork was born out of a need in Budgie 10 series to have a drop-in replacement of gnome-screensaver for use in Budgie, after upstream [indicated](https://mail.gnome.org/archives/desktop-devel-list/2016-July/msg00030.html) the appropriate continued use of gnome-screensaver is a fork, as they are not planning on taking patches. The overwhelming majority of patches are ported from Debian and/or Ubuntu's package-fork of gnome-screensaver to allow a simple drop-in replacement for the gnome-screensaver package. This repository and its respective software should largely be considered as being in "maintenance mode", only updating to resolve FTBFS issues or reflect changes in the GNOME Stack.

## Compiling

Budgie Screensaver makes use of GNU Autotools for configuration, compilation, and installation. As such, the standard method is to:

1. Use `autogen.sh` when `configure` is not available, otherwise use configure.
2. `make`
3. `make install`

## LICENSE

Budgie Screensaver is licensed under GPL-2.0-only.