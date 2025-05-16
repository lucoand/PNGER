# PNGER
v0.01

A CLI png viewer project originally made for boot.dev

Currently the application is quite limited.   I have only added support for 32 bit RGBA png files but more functionality is on the way.
Additionally, it currently only supports the X11 window manager.

Included Makefile allows for simple compilation (assumes gcc compiler):

> make

Object files and executable will be created in /build directory.  The /build directory will be generated if not present.

> make clean

This will remove the build directory allowing for a fresh build.

If you prefer, you can use this command, which will build pnger in the project root:

> gcc -o pnger src/*.c -lX11 -lm -lz

Made by Lucoa, using X11, zlib, and standard libraries.


