# PNGER
v0.03

A CLI png viewer for Linux made in C; project originally made for boot.dev

Currently PNGER can display files created in RGB and RGBA modes, and supports both 8-bit and 16-bit (downsampled) pixel channels.
I transitioned from X11 to OpenGL to allow for better alpha channel support.

THIS IS A WORK IN PROGRESS.  A variety of features are planned to be added:
- Support for grayscale and pallete color modes.
- Support for ancillary png chunks.
- Opening different png files from within the application once loaded.
- Actual GUI elements.

## BUILD

Using CMake (from project root):

> mkdir build && cd build

> cmake ..

> make

### USAGE

PNGER expects a single command line argument, and currently can only open the file it is provided:

> ./pnger my_image.png

To close PNGER, press ESC.

For now, the file need not have a .png extension.  As long as the file is a valid png image, PNGER will at least attempt to open it.

Made by Lucoa, using glfw, glad, zlib, and standard libraries.


