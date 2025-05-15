#include "image.h"
#include "png.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Requires one argument.  Should be a png file.\n");
    return 1;
  }
  if (argc > 2) {
    printf("Should only be one argument.\n");
    return 1;
  }
  // printf("%s\n", argv[1]);
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }
  PNG *png = decode_PNG(f);
  if (!png) {
    fclose(f);
    return 1;
  }

  uint32_t width = png->header->width;
  uint32_t height = png->header->height;

  for (int i = 0; i < width * height; i++) {
    if (png->pixels[i].r != 0xff || png->pixels[i].g != 0xff ||
        png->pixels[i].b != 0xff) {
      printf("Non-white pixel found! Pixel number %d\n", i);
      break;
    }
  }

	print_pixel(png->pixels[6686]);

  Display *d;
  Window w;
  XEvent e;
  int s;

  d = XOpenDisplay(NULL);
  if (d == NULL) {
    fprintf(stderr, "Cannot open display\n");
    free_PNG(png);
    fclose(f);
    exit(1);
  }

  s = DefaultScreen(d);
  w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, width, height, 1,
                          BlackPixel(d, s), BlackPixel(d, s));

  XStoreName(d, w, "PNGER v 0.01");
  XSelectInput(d, w, ExposureMask | KeyPressMask);
  XMapWindow(d, w);

  Atom delWindow = XInternAtom(d, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(d, w, &delWindow, 1);

  GC gc = XCreateGC(d, w, 0, NULL);
  XColor color;
  Colormap colormap = DefaultColormap(d, 0);
  XAllocNamedColor(d, colormap, "red", &color, &color);
  XImage *img = create_img(d, s, png);
  while (1) {
    XNextEvent(d, &e);
    if (e.type == Expose) {
      XSetForeground(d, gc, BlackPixel(d, s));
      XFillRectangle(d, w, gc, 0, 0, width, height);
      XPutImage(d, w, gc, img, 0, 0, 0, 0, width, height);
    }
    if (e.type == KeyPress) {
      break;
    }
    if (e.type == ClientMessage) {
      if ((Atom)e.xclient.data.l[0] == delWindow) {
        break;
      }
    }
  }

  XDestroyImage(img);
  XFreeGC(d, gc);
  XDestroyWindow(d, w);
  XCloseDisplay(d);
  free_PNG(png);
  fclose(f);
  return 0;
}
