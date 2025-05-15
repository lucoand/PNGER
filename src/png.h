#ifndef PNG_H
#define PNG_H
#include "main.h"

typedef enum {
  UNKNOWN,
  GS,
  RGB,
  PALETTE,
  GSA,
  RGBA,
} PixelFormat;

/**
 * Holds IHDR chunk data. See https://www.rfc-editor.org/rfc/rfc2083#page-15
 */
typedef struct {
  uint32_t width;
  uint32_t height;
  PixelFormat pixel_format;
  uint8_t bit_depth;
  uint8_t color_type;
  uint8_t compression_method;
  uint8_t filter_method;
  uint8_t interlace_method;
} PNG_IHDR;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
} PIXEL;

typedef struct {
  PNG_IHDR *header;
  PIXEL *pixels;
} PNG;

PNG *decode_PNG(FILE *f);
void print_pixel(PIXEL p);
void free_PNG(PNG *p);

#endif //PNG
