#include "image.h"

XImage *create_img(Display *d, int screen, PNG *p);

void img_RGBA(XImage *img, uint32_t width, uint32_t height, PIXELRGBA *pixels);
void img_RGB(XImage *img, uint32_t width, uint32_t height, PIXELRGB *pixels);

XImage *create_img(Display *d, int screen, PNG *p) {
  if (!d || !p) {
    return NULL;
  }
  uint32_t width = p->header->width;
  uint32_t height = p->header->height;
  PixelFormat pixel_format = p->header->pixel_format;
  uint8_t bit_depth = p->header->bit_depth;
  void *pixels = p->pixels;
  size_t bytes_per_pixel;
  switch (pixel_format) {
  case RGBA:
    if (bit_depth == 8) {
      bytes_per_pixel = 4;
    } else {
      printf("Bit depth 16 for RGBA not yet implemented.\n");
      return NULL;
    }
    break;
  case RGB:
    if (bit_depth == 8) {
      // even though this isn't "correct", we seem to need
      // this value to be 4 for quantum of 32.
      // Quirky.
      bytes_per_pixel = 4;
    } else {
      printf("Bit depth 16 for RGB not yet implemented.\n");
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
  if (bit_depth == 8 && pixel_format == RGBA) {
    img_RGBA(img, width, height, (PIXELRGBA *)pixels);
  } else if (bit_depth == 8 && pixel_format == RGB) {
    img_RGB(img, width, height, (PIXELRGB *)pixels);
  } else {
    printf("Unsupported pixel format and bit depth\n");
    XDestroyImage(img);
    img = NULL;
    return NULL;
  }
  // printf("image created.\n");
  return img;
}

void img_RGBA(XImage *img, uint32_t width, uint32_t height, PIXELRGBA *pixels) {
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      unsigned long pixel;
      if (pixels[y * width + x].a == 0) {
        pixel = 0L;
      } else {
        pixel = (pixels[y * width + x].r << 16) |
                (pixels[y * width + x].g << 8) | pixels[y * width + x].b;
      }
      XPutPixel(img, x, y, pixel);
    }
  }
}

void img_RGB(XImage *img, uint32_t width, uint32_t height, PIXELRGB *pixels) {
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      unsigned long pixel;
      pixel = (pixels[y * width + x].r << 16) | (pixels[y * width + x].g << 8) |
              pixels[y * width + x].b;
      XPutPixel(img, x, y, pixel);
    }
  }
}
