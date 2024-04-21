DeforaOS VNCViewer
==================

About VNCViewer
---------------

This program is meant to be used as a client for the VNC remote desktop sharing
protocol.

This project is originally based on an example file from the gtk-vnc library
(https://wiki.gnome.org/Projects/gtk-vnc).

VNCViewer is part of the DeforaOS Project, found at https://www.defora.org/.

Compiling VNCViewer
-------------------

The current requirements for compiling VNCViewer are as follows:

 * Gtk+ 2.4 or later, or Gtk+ 3.0 or later (the default)
 * DeforaOS libDesktop
 * the gtk-vnc library
 * the gvncpulse library for audio support (optional, default)
 * an implementation of `make`
 * gettext (libintl) for translations

With these installed, the following command should be enough to compile
VNCViewer on most systems:

    $ make

The following command will then install VNCViewer:

    $ make install

To install (or package) VNCViewer in a different location:

    $ make clean
    $ make PREFIX="/another/prefix" install

VNCViewer also supports `DESTDIR`, to be installed in a staging directory; for
instance:

    $ make DESTDIR="/staging/directory" PREFIX="/another/prefix" install

On some systems, the Makefiles shipped can be re-generated accordingly thanks to
the DeforaOS configure tool.

The compilation process supports a number of options, such as PREFIX and DESTDIR
for packaging and portability, or OBJDIR for compilation outside of the source
tree.

Distributing VNCViewer
----------------------

DeforaOS VNCViewer is subject to the terms of the LGPL license, version 2.0.
Please see the `COPYING` file for more information.
