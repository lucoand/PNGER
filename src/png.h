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
} PIXELRGBA;

typedef struct {
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t a;
} PIXELRGBA16;

typedef struct {
  uint8_t gs;
  uint8_t a;
} PIXELGSA;

typedef struct {
  uint16_t gs;
  uint16_t a;
} PIXELGSA16;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} PIXELPLTE;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} PIXELRGB;

typedef struct {
  uint16_t r;
  uint16_t g;
  uint16_t b;
} PIXELRGB16;

typedef struct {
  uint8_t gs;
} PIXELGS;

typedef struct {
  uint16_t gs;
} PIXELGS16;

typedef struct {
  unsigned a : 4;
  unsigned b : 4;
} PIXELGS4;

typedef struct {
  unsigned a : 2;
  unsigned b : 2;
  unsigned c : 2;
  unsigned d : 2;
} PIXELGS2;

typedef struct {
  unsigned a : 1;
  unsigned b : 1;
  unsigned c : 1;
  unsigned d : 1;
  unsigned e : 1;
  unsigned f : 1;
  unsigned g : 1;
  unsigned h : 1;
} PIXELGS1;

typedef struct {
  PNG_IHDR *header;
  uint8_t *pixels;
  size_t bytes_per_row;
} PNG;

PNG *decode_PNG(FILE *f);
void print_pixel(PIXELRGBA p);
void free_PNG(PNG *p);

#endif // PNG
