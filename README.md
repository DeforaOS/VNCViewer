DeforaOS VNCViewer
==================

About VNCViewer
---------------

This program is meant to be used as a client for the VNC remote desktop sharing
protocol.

This project is originally based on an example file from the gtk-vnc library
(https://wiki.gnome.org/Projects/gtk-vnc).

Compiling Panel
---------------

The current requirements for compiling Panel are as follows:
 * Gtk+ 2.4 or later, or Gtk+ 3.0 or later
 * the gtk-vnc library
 * an implementation of `make`

With these installed, the following command should be enough to compile
VNCViewer on most systems:

    $ make

VNCViewer can then be installed as follows:

    $ make install

On some systems, the Makefiles shipped can be re-generated accordingly thanks to
the DeforaOS configure tool.

The compilation process supports a number of options, such as PREFIX and DESTDIR
for packaging and portability, or OBJDIR for compilation outside of the source
tree.

Distributing Panel
------------------

DeforaOS Panel is subject to the terms of the LGPL license, version 2.0. Please
see the `COPYING` file for more information.
