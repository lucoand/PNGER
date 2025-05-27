# PNGER
v0.04

A CLI png viewer for Linux made in C; project originally made for boot.dev

Currently PNGER is compatible with all png color modes, filter methods, and bit depths (with downsampling of 16 bit samples to 8 bit).
However, it only supports interlace method 0 (non-interlaced).  Color space is assumed to be sRGB.

## REQUIREMENTS
OS: Linux
OpenGL 3.30+

THIS IS A WORK IN PROGRESS.  A variety of features are planned to be added:
- Support for interlaced pngs.
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

For now, the file need not have a .png extension.  As long as the file has a valid png signature, PNGER will at least attempt to open it.

Color accuracy is most likely lacking.  I did however compare my output against GIMP and at least on my own system they appear to match.

Made by Lucoa, using glfw, glad, zlib, and standard libraries.


