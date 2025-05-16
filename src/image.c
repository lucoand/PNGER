#include "image.h"

XImage *create_img(Display *d, int screen, PNG *p) {
  if (!d || !p) {
    return NULL;
  }
  uint32_t width = p->header->width;
  uint32_t height = p->header->height;
  size_t bytes_per_pixel;
  switch (p->header->pixel_format) {
  case RGBA:
    if (p->header->bit_depth == 8) {
      bytes_per_pixel = 4;
    } else {
      printf("Bit depth 16 for RGBA not yet implemented.\n");
      return NULL;
    }
    break;
  default:
    printf("Pixel format not yet implemented.\n");
    return NULL;
    break;
  }

  XImage *img =
      XCreateImage(d, DefaultVisual(d, screen), DefaultDepth(d, screen),
                   ZPixmap, 0, malloc(width * height * bytes_per_pixel), width,
                   height, 32, bytes_per_pixel * width);
  if (!img) {
    printf("Unable to allocate image for drawing.\n");
    return NULL;
  }

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      unsigned long pixel;
      if (p->pixels[y * width + x].a == 0) {
        pixel = 0L;
      } else {
        pixel = (p->pixels[y * width + x].r << 16) |
                (p->pixels[y * width + x].g << 8) | p->pixels[y * width + x].b;
      }
      XPutPixel(img, x, y, pixel);
      // XPutPixel(img, x, y, 0xFF0000);
    }
  }
  // printf("image created.\n");
  return img;
}
