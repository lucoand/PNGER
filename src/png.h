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

typedef struct {
  uint8_t r, g, b;
} PLTE;

/**
 * Holds IHDR chunk data. See https://www.rfc-editor.org/rfc/rfc2083#page-15
 */
typedef struct {
  PLTE *pal;
  uint32_t width;
  uint32_t height;
  uint32_t gamma; // gamma * 100000
  PixelFormat pixel_format;
  uint8_t bit_depth;
  uint8_t color_type;
  uint8_t compression_method;
  uint8_t filter_method;
  uint8_t interlace_method;
  uint8_t num_pal;
  bool has_plte;
  bool has_gama;
} PNG_IHDR;

typedef struct {
  PNG_IHDR *header;
  uint8_t *pixels;
  size_t bytes_per_row;
} PNG;

PNG *decode_PNG(FILE *f);
void free_PNG(PNG *p);

#endif // PNG
