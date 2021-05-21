
Debian
====================
This directory contains files used to package bytzd/bytz-qt
for Debian-based Linux systems. If you compile bytzd/bytz-qt yourself, there are some useful files here.

## bytz: URI support ##


bytz-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install bytz-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your bytz-qt binary to `/usr/bin`
and the `../../share/pixmaps/bytz128.png` to `/usr/share/pixmaps`

bytz-qt.protocol (KDE)

