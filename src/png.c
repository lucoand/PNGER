#include "png.h"
#include <stddef.h>
#include <stdint.h>

#define MAX_DATA_LEN 2147483647
#define IHDR_LEN 13
#define PROPERTY_BIT 0b100000

const unsigned char PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};
unsigned long crc_table[256];
int crc_table_computed = 0;
bool idat_start = false;
bool idat_end = false;

/**
 * Struct with two fields for proper validation of a png chunk length.
 * Made to work with the get_chunk_length() function.
 * Field "len" is used to record the length of the chunk.
 * Field "valid" is set to true if the chunk has a proper length.
 * "valid" will be false if the length fails to be read properly or if the value
 * exceeds MAX_DATA_LEN, as per the png spec.
 */
typedef struct {
  uint32_t len;
  bool valid;
} LENGTH;

typedef struct {
  char *type;
  uint32_t length;
  void *data;
  bool ancillary;
} CHUNK;

uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c);

// void *allocate_PIXELs(PNG_IHDR *hdr) {
//   size_t num_pixels = hdr->height * hdr->width;
//   size_t size;
//   uint8_t bit_depth = hdr->bit_depth;
//
//   switch (hdr->pixel_format) {
//   case RGBA:
//     if (bit_depth == 8) {
//       size = sizeof(PIXELRGBA);
//       break;
//     }
//     if (bit_depth == 16) {
//       size = sizeof(PIXELRGBA16);
//       break;
//     }
//     printf("Incompatible bit depth %d for RGBA pixel format.\n", bit_depth);
//     return NULL;
//     break;
//   case GSA:
//     if (bit_depth == 8) {
//     }
//   }
//
//   void *pixels = calloc(num_pixels, size);
//   if (!pixels) {
//     printf("Allocation error: pixels.\n");
//     return NULL;
//   }
//   return pixels;
// }

void make_crc_table(void) {
  unsigned long c;
  int n, k;
  for (n = 0; n < 256; n++) {
    c = (unsigned long)n;
    for (k = 0; k < 8; k++) {
      if (c & 1)
        c = 0xedb88320L ^ (c >> 1);
      else
        c = c >> 1;
    }
    crc_table[n] = c;
  }
  crc_table_computed = 1;
  // printf("CRC table computed\n");
}

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len) {
  unsigned long c = crc;
  int n;

  if (!crc_table_computed)
    make_crc_table();
  for (n = 0; n < len; n++) {
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  return c;
}

unsigned long crc(unsigned char *buf, int len) {
  return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

void invalid_png();
/**
 * Accepts a FILE pointer.
 * Checks the signature of the FILE,
 * to see if it is valid for png.
 * **THIS MOVES THE INTERNAL READ/WRITE POINTER FOR THE FILE.**
 * Returns 1 if the file has a valid png signature, 0 otherwise.
 */
int get_sig(FILE *fp) {
  unsigned char sig[8];
  if (fread(sig, 1, 8, fp) != 8)
    return 0;

  return memcmp(sig, PNG_SIGNATURE, 8) == 0;
}

/**
 * Accepts a FILE pointer.
 * Returns the size in bytes of the file,
 * without affecting internal FILE struct pointers.
 */
long get_file_size(FILE *fp) {
  long size;

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  rewind(fp);

  return size;
}

/**
 * Accepts a FILE pointer that is ready
 * to have its png chunk length checked,
 * typically after the signature has been
 * validated or after an entire chunk has been
 * processed.
 * Returns a LENGTH struct.  This
 * process can fail, check the LENGTH.valid bool
 * to be certain of validity.
 */
LENGTH get_chunk_length(FILE *fp) {
  LENGTH len;
  if (fread(&len.len, 4, 1, fp) != 1) {
    len.valid = false;
    return len;
  }

  len.len = ntohl(len.len);

  if (len.len > MAX_DATA_LEN) {
    len.valid = false;
    return len;
  }

  len.valid = true;
  return len;
}

char *get_chunk_type(unsigned char **buf) {
  char *t = (char *)calloc(5, 1);
  if (!t) {
    return NULL;
  }
  memcpy(t, *buf, 4);
  *buf = *buf + 4;
  return t;
}

void bad_header(PNG_IHDR *hdr) {
  hdr->width = 0;
  hdr->height = 0;
  return;
}

void invalid_crc(void) { printf("CRC failed.\n"); }

bool get_chunk_bytes(void *buf, size_t num, FILE *fp) {
  if (fread(buf, num, 1, fp) != 1) {
    invalid_png();
    return false;
  }
  return true;
}

void get_buffer(unsigned char **buf, void *field, int len) {
  memcpy(field, *buf, len);
  *buf = *buf + len;
}

bool get_crc(unsigned long *crc, FILE *fp) {
  if (fread(crc, 4, 1, fp) != 1) {
    invalid_png();
    return false;
  }
  *crc = ntohl(*crc);
  // printf("inside get_crc: %lu\n", *crc);
  return true;
}

void get_IHDR(unsigned char **buf, PNG_IHDR *hdr) {
  get_buffer(buf, &hdr->width, 4);
  hdr->width = ntohl(hdr->width);
  get_buffer(buf, &hdr->height, 4);
  hdr->height = ntohl(hdr->height);
  get_buffer(buf, &hdr->bit_depth, 1);
  get_buffer(buf, &hdr->color_type, 1);
  get_buffer(buf, &hdr->compression_method, 1);
  get_buffer(buf, &hdr->filter_method, 1);
  get_buffer(buf, &hdr->interlace_method, 1);
}

CHUNK get_chunk(FILE *fp) {
  LENGTH len = get_chunk_length(fp);
  CHUNK chunk = {0};
  if (!len.valid) {
    invalid_png();
    return chunk;
  }
  chunk.length = len.len;

  unsigned char *original = calloc((size_t)(len.len + 4), 1);
  unsigned char *buf = original;
  if (!get_chunk_bytes(buf, len.len + 4, fp)) {
    invalid_png();
    free(original);
    buf = NULL;
    original = NULL;
    chunk.length = 0;
    return chunk;
  }

  unsigned long crc_computed = crc(buf, (int)(len.len + 4));
  unsigned long ccrc;
  unsigned long *ccrc_ptr = &ccrc;
  if (!get_crc(ccrc_ptr, fp)) {
    invalid_png();
    free(original);
    buf = NULL;
    original = NULL;
    ccrc_ptr = NULL;
    chunk.length = 0;
    return chunk;
  }
  if (crc_computed != ccrc) {
    invalid_crc();
    free(original);
    buf = NULL;
    original = NULL;
    ccrc_ptr = NULL;
    chunk.length = 0;
    return chunk;
  }
  ccrc_ptr = NULL;

  // printf("buf before get_chunk_type: %p\n", buf);
  chunk.type = get_chunk_type(&buf);
  if (!chunk.type || strlen(chunk.type) != 4) {
    invalid_png();
    free(original);
    buf = NULL;
    original = NULL;
    chunk.length = 0;
    return chunk;
  }
  // printf("buf after get_chunk_type: %p\n", buf);
  // char *t = get_chunk_type(&buf);
  // if (!t) {
  //   invalid_png();
  //   free(original);
  //   buf = NULL;
  //   original = NULL;
  //   chunk.length = 0;
  //   return chunk;
  // }
  //
  // memcpy(chunk.type, t, 5);
  // free(t);
  // t = NULL;

  if ((chunk.type[0] & PROPERTY_BIT) == PROPERTY_BIT) {
    chunk.ancillary = true;
  }
  if (chunk.ancillary == true) {
    free(original);
    buf = NULL;
    original = NULL;
    return chunk;
  }

  if (strcmp(chunk.type, "IHDR") == 0) {
    PNG_IHDR *hdr = (PNG_IHDR *)calloc(1, sizeof(PNG_IHDR));
    if (!hdr) {
      printf("Error allocating memory\n");
      free(original);
      buf = NULL;
      original = NULL;
      chunk.length = 0;
      return chunk;
    }
    get_IHDR(&buf, hdr);

    chunk.data = hdr;
    free(original);
    original = NULL;
    buf = NULL;
    return chunk;
  }

  if (strcmp(chunk.type, "IDAT") == 0) {
    // printf("IDAT chunk length %d\n", chunk.length);
    unsigned char *data = (unsigned char *)calloc((size_t)chunk.length, 1);
    if (!data) {
      printf("Error allocating memory\n");
      free(original);
      buf = NULL;
      original = NULL;
      chunk.length = 0;
      return chunk;
    }
    memcpy(data, buf, (size_t)chunk.length);
    chunk.data = data;
    free(original);
    original = NULL;
    buf = NULL;
    return chunk;
  }

  if (strcmp(chunk.type, "IEND") == 0) {
    chunk.data = NULL;
    if (chunk.length != 0) {
      printf("IEND chunk length nonzero! Bad PNG.\n");
    }
    free(original);
    original = NULL;
    buf = NULL;
    return chunk;
  }

  free(original);
  original = NULL;
  buf = NULL;
  chunk.length = 0;
  return chunk;
}

void invalid_png() { printf("File is not a valid png!\n"); }

void print_IHDR(PNG_IHDR *hdr) {
  printf("Header contents:\n");
  printf("----------------\n");
  printf("Width:              %d\n", hdr->width);
  printf("Height:             %d\n", hdr->height);
  printf("Bit Depth:          %d\n", hdr->bit_depth);
  printf("Color type:         %d\n", hdr->color_type);
  printf("Compression method: %d\n", hdr->compression_method);
  printf("Filter method:      %d\n", hdr->filter_method);
  printf("Interlace method:   %d\n", hdr->interlace_method);
  printf("Pixel Format: ");
  switch (hdr->pixel_format) {
  case GS:
    printf("Grayscale\n");
    break;
  case RGB:
    printf("RGB\n");
    break;
  case PALETTE:
    printf("Palette");
    break;
  case GSA:
    printf("Grayscale w/Alpha\n");
    break;
  case RGBA:
    printf("RGB w/Alpha\n");
    break;
  default:
    printf("Unknown - This should not be reachable!\n");
    break;
  }
  printf("----------------\n");
}

void free_chunk_data(CHUNK *ch) {
  if (ch == NULL) {
    return;
  }
  if (ch->data != NULL) {
    free(ch->data);
    ch->data = NULL;
  }
}

void free_chunks_data(CHUNK *ch, int num) {
  if (ch == NULL) {
    return;
  }
  for (int i = 0; i < num; i++) {
    free_chunk_data(&ch[i]);
  }
}

void free_chunks(CHUNK *ch, int num) {
  free_chunks_data(ch, num);
  free(ch);
}

void print_chunk(CHUNK *ch) {
  if (!ch) {
    printf("Null chunk\n");
    return;
  }
  printf("Chunk Type: %s\n", ch->type);
  printf("Chunk Length: %d\n", ch->length);
  printf("Ancillary: %s\n", ch->ancillary ? "true" : "false");
  printf("----------------\n");
  if (strcmp(ch->type, "IHDR") == 0) {
    PNG_IHDR *hdr = ch->data;
    print_IHDR(hdr);
    return;
  }
  if (strcmp(ch->type, "pHYs") == 0) {
    return;
  }
  if (strcmp(ch->type, "IDAT") == 0) {
    unsigned char *data = ch->data;
    printf("Compression method/flags code: %x\n", data[0]);
    printf("Additional flags/check bits: %x\n", data[1]);
    printf("----------------\n");
    return;
  }

  if (strcmp(ch->type, "IEND") == 0) {
    return;
  }

  printf("Unknown chunk type\n");
}

bool verify_IHDR_data(PNG_IHDR *hdr) {
  if (hdr->width == 0 || hdr->height == 0) {
    printf("Invalid resolution.\n");
    return false;
  }
  if (hdr->compression_method != 0) {
    printf("Invalid compression method.\n");
    return false;
  }
  if (hdr->filter_method != 0) {
    printf("Invalid filter method.\n");
    return false;
  }
  if (hdr->interlace_method > 1) {
    printf("Invalid interlace method.\n");
    return false;
  }
  return true;
}

PixelFormat get_pixel_format(PNG_IHDR *hdr) {
  bool bd8_16 = hdr->bit_depth == 8 || hdr->bit_depth == 16;
  if (hdr->color_type == 6 && bd8_16) {
    return RGBA;
  }
  if (hdr->color_type == 4 && bd8_16) {
    return GSA;
  }
  if (hdr->color_type == 2 && bd8_16) {
    return RGB;
  }
  bool bd1_2_4 =
      hdr->bit_depth == 1 || hdr->bit_depth == 2 || hdr->bit_depth == 4;
  if (hdr->color_type == 3 && (bd1_2_4 || hdr->bit_depth == 8)) {
    return PALETTE;
  }
  if (hdr->color_type == 0 && (bd1_2_4 || bd8_16)) {
    return GS;
  }
  return UNKNOWN;
}

size_t get_buffer_size(uint32_t height, uint32_t width, uint8_t bit_depth,
                       PixelFormat pixel_format, size_t *bytes_per_row) {
  int samples_per_pixel;
  size_t buffer_size;
  switch (pixel_format) {
  case GS:
    samples_per_pixel = 1;
    break;
  case RGB:
    samples_per_pixel = 3;
    break;
  case PALETTE:
    *bytes_per_row = width + 1;
    buffer_size = height * (width + 1);
    return buffer_size;
    break;
  case GSA:
    samples_per_pixel = 2;
    break;
  case RGBA:
    samples_per_pixel = 4;
    break;
  default:
    return 0;
    break;
  }
  switch (bit_depth) {
  case 16:
    *bytes_per_row = width * samples_per_pixel * 2 + 1;
    break;
  case 8:
    *bytes_per_row = width * samples_per_pixel + 1;
    break;
  case 4:
    *bytes_per_row = (size_t)ceil((double)width / 2) + 1;
    break;
  case 2:
    *bytes_per_row = (size_t)ceil((double)width / 4) + 1;
    break;
  case 1:
    *bytes_per_row = (size_t)ceil((double)width / 8) + 1;
    break;
  default:
    return 0;
    break;
  }

  buffer_size = height * *bytes_per_row;
  // printf("buffer_size within get_buffer_size: %zu\n", buffer_size);
  return buffer_size;
}

void test_uncompress(void) {
  // zlib compressed "hello\n"
  unsigned char sample_data[] = {0x78, 0x9c, 0xcb, 0x48, 0xcd, 0xc9, 0xc9,
                                 0x57, 0x28, 0xcf, 0x2f, 0xca, 0x49, 0x01,
                                 0x00, 0x1a, 0x0b, 0x04, 0x5d};
  unsigned long sample_size = sizeof(sample_data);
  unsigned char buffer[64];
  uLongf buf_len = sizeof(buffer);

  int r = uncompress(buffer, &buf_len, sample_data, sample_size);
  printf("Return value: %d\n", r);
}

int decompress_pixels(const unsigned char *compressed_data,
                      size_t compressed_size, PNG_IHDR *hdr, uint8_t **out_data,
                      size_t *out_size, size_t *bytes_per_row) {
  if (!compressed_data || !hdr || !out_data || !out_size || !bytes_per_row) {
    printf("Null pointer passed to decompress pixels.\n");
    return -1;
  }
  size_t buffer_size = get_buffer_size(hdr->height, hdr->width, hdr->bit_depth,
                                       hdr->pixel_format, bytes_per_row);
  *out_data = malloc(buffer_size);
  if (!*out_data) {
    fprintf(stderr, "malloc failed for buffer size %zu\n", buffer_size);
    return -1;
  }

  int z_result =
      uncompress(*out_data, &buffer_size, compressed_data, compressed_size);
  if (z_result != Z_OK) {
    fprintf(stderr, "uncompress result: %d\n", z_result);
    fflush(stderr);
    free(*out_data);
    *out_data = NULL;
    return z_result;
  }
  *out_size = buffer_size;
  return Z_OK;
}

/**
 * a = left, b = up, c = up left
 **/
uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c) {
  int aa = a;
  int bb = b;
  int cc = c;
  int p = aa + bb - cc;
  int pa = abs(p - aa);
  int pb = abs(p - bb);
  int pc = abs(p - cc);
  if (pa <= pb && pa <= pc) {
    return a;
  }
  if (pb <= pc) {
    return b;
  }
  return c;
}

void get_RGBA_pixels(uint8_t *data, PNG_IHDR *hdr, PIXELRGBA *pixels) {
  // four bytes per pixel
  int bytes_per_pixel = 4;
  size_t offset = hdr->width * bytes_per_pixel;
  for (uint32_t i = 0; i < hdr->height; i++) {
    for (uint32_t j = 0; j < hdr->width; j++) {
      uint32_t pix_offset = hdr->width * i + j;
      uint32_t byte_offset = offset * i + j * bytes_per_pixel;
      pixels[pix_offset].r = data[byte_offset];
      pixels[pix_offset].g = data[byte_offset + 1];
      pixels[pix_offset].b = data[byte_offset + 2];
      pixels[pix_offset].a = data[byte_offset + 3];
    }
  }
}

void get_RGBA16_pixels(uint8_t *data, PNG_IHDR *hdr, PIXELRGBA16 *pixels) {
  int bytes_per_pixel = 8;
  size_t offset = hdr->width * bytes_per_pixel;
  for (uint32_t i = 0; i < hdr->height; i++) {
    for (uint32_t j = 0; j < hdr->width; j++) {
      uint32_t pix_offset = hdr->width * i + j;
      uint32_t byte_offset = offset * i + j * bytes_per_pixel;
      uint16_t r = (uint16_t)data[byte_offset] << 8 | data[byte_offset + 1];
      uint16_t g = (uint16_t)data[byte_offset + 2] << 8 | data[byte_offset + 3];
      uint16_t b = (uint16_t)data[byte_offset + 4] << 8 | data[byte_offset + 5];
      uint16_t a = (uint16_t)data[byte_offset + 6] << 8 | data[byte_offset + 7];
      pixels[pix_offset].r = r;
      pixels[pix_offset].g = g;
      pixels[pix_offset].b = b;
      pixels[pix_offset].a = a;
    }
  }
}

void get_RGB_pixels(uint8_t *data, PNG_IHDR *hdr, PIXELRGB *pixels) {
  // three bytes per pixel
  int bytes_per_pixel = 3;
  size_t offset = hdr->width * bytes_per_pixel;
  for (uint32_t i = 0; i < hdr->height; i++) {
    for (uint32_t j = 0; j < hdr->width; j++) {
      uint32_t pix_offset = hdr->width * i + j;
      uint32_t byte_offset = offset * i + j * bytes_per_pixel;
      pixels[pix_offset].r = data[byte_offset];
      pixels[pix_offset].g = data[byte_offset + 1];
      pixels[pix_offset].b = data[byte_offset + 2];
    }
  }
}

void get_RGB16_pixels(uint8_t *data, PNG_IHDR *hdr, PIXELRGB16 *pixels) {
  int bytes_per_pixel = 6;
  size_t offset = hdr->width * bytes_per_pixel;
  for (uint32_t i = 0; i < hdr->height; i++) {
    for (uint32_t j = 0; j < hdr->width; j++) {
      uint32_t pix_offset = hdr->width * i + j;
      uint32_t byte_offset = offset * i + j * bytes_per_pixel;
      uint16_t r = (uint16_t)data[byte_offset] << 8 | data[byte_offset + 1];
      uint16_t g = (uint16_t)data[byte_offset + 2] << 8 | data[byte_offset + 3];
      uint16_t b = (uint16_t)data[byte_offset + 4] << 8 | data[byte_offset + 5];
      pixels[pix_offset].r = r;
      pixels[pix_offset].g = g;
      pixels[pix_offset].b = b;
    }
  }
}

/** This takes in the raw unfiltered data with bit depth 16
 * and applies an srgb approximation to it before downsampling
 * to 8 bit
 */
uint8_t *convert_16_to_8(uint8_t *data, size_t num_bytes, PixelFormat format) {
  bool is_alpha = false;
  size_t alpha = 0;
  if (format == RGBA || format == GSA) {
    is_alpha = true;
  }
  if (format == RGBA) {
    alpha = 4;
  } else if (format == GSA) {
    alpha = 2;
  }

  uint8_t *new_data = malloc(num_bytes / 2);
  // maps color to allow for good 16 bit approximation
  for (size_t i = 0; i < num_bytes / 2; i++) {
    uint16_t sample = (uint16_t)data[i * 2] << 8 | data[(i * 2) + 1];
    if (is_alpha && i % alpha == alpha - 1) {
      new_data[i] = sample / 257;
      continue;
    }
    float linear = sample / 65535.0f;
    float srgb = powf(linear, 1.0f / 2.2f);
    new_data[i] = (uint8_t)roundf(srgb * 255.0f);
  }
  free(data);
  return new_data;
}

void apply_srgb(uint8_t *pixels, size_t size, PixelFormat format) {
  bool is_alpha = false;
  size_t alpha = 0;
  if (format == RGBA || format == GSA) {
    is_alpha = true;
  }
  if (format == RGBA) {
    alpha = 4;
  } else if (format == GSA) {
    alpha = 2;
  }

  for (int i = 0; i < size; i++) {
    if (is_alpha && i % alpha == alpha - 1) {
      continue;
    }
    float linear = pixels[i] / 255.0f;
    float srgb = powf(linear, 1.0f / 2.2f);
    pixels[i] = (uint8_t)roundf(srgb * 255.0f);
  }
}

bool get_pixels(uint8_t *data, PNG_IHDR *hdr, void *pixels) {
  if (!data || !hdr || !pixels) {
    fprintf(stderr, "Null pointer passed into get_pixels.\n");
    return false;
  }
  switch (hdr->pixel_format) {
  case RGBA:
    if (hdr->bit_depth != 8 && hdr->bit_depth != 16) {
      printf(
          "Unsupported bit depth for RGBA.  This message shouldn't appear.\n");
      return false;
    }
    // if (hdr->bit_depth == 8) {
    get_RGBA_pixels(data, hdr, (PIXELRGBA *)pixels);
    return true;
    // }
    // if (hdr->bit_depth == 16) {
    //   get_RGBA16_pixels(data, hdr, (PIXELRGBA16 *)pixels);
    //   return true;
    // }
    printf("Couldn't get pixels.  This message shouldn't appear.\n");
    return false;
    break;
  case RGB:
    if (hdr->bit_depth != 8 && hdr->bit_depth != 16) {
      printf(
          "Unsupported bit depth for RGB.  This message shouldn't appear.\n");
    }
    // if (hdr->bit_depth == 8) {
    get_RGB_pixels(data, hdr, (PIXELRGB *)pixels);
    return true;
    // }
    // if (hdr->bit_depth == 16) {
    //   get_RGB16_pixels(data, hdr, (PIXELRGB16 *)pixels);
    //   return true;
    // }
    printf("Couldn't get pixels.  This message shouldn't appear.\n");
    return false;
    break;
  default:
    printf("Pixel format not yet implemented: %d\n", hdr->pixel_format);
    return false;
    break;
  }
}

bool get_pixels2(uint8_t *data, PNG_IHDR *hdr, uint8_t **pixels) {
  // pixels should be an uninitialized pointer i think
  if (!data || !hdr) {
    fprintf(stderr, "Null pointer passed into get_pixels.\n");
    return false;
  }
  uint8_t bit_depth = hdr->bit_depth;
  switch (hdr->pixel_format) {
  case RGB:
  case RGBA:
    if (bit_depth == 8 || bit_depth == 16) {
      *pixels = data;
      return true;
    }
    break;
  case PALETTE:
    printf("Pallate images not yet implemented.\n");
    break;
  case GS:
  case GSA:
    printf("Grayscale images not yet implemented.\n");
    break;
  default:
    printf("Unknown pixel format.  This message probably shouldn't print.\n");
    break;
  }
  free(data);
  return false;
}

void print_pixel(PIXELRGBA p) {
  printf("Pixel Data: ");
  printf("R=%02x G=%02x B=%02x A=%02x\n", p.r, p.g, p.b, p.a);
}

void free_PNG(PNG *p) {
  if (!p) {
    return;
  }
  if (p->pixels) {
    free(p->pixels);
  }
  if (p->header) {
    free(p->header);
  }
  p->pixels = NULL;
  p->header = NULL;
  p = NULL;
}

bool unfilter_data(uint8_t *raw_data, uint8_t *out_data, PNG_IHDR *hdr,
                   size_t bytes_per_row) {
  uint32_t bps; // bytes per sample, minimum 1 (for sample depth < 8 should
                // still be fine)
  uint32_t spp; // samples per pixel, minimum 1
  uint8_t bd = hdr->bit_depth;
  if (bd == 16) {
    bps = 2;
  } else {
    bps = 1;
  }

  switch (hdr->pixel_format) {
  case RGBA:
    spp = 4;
    break;
  case GSA:
    spp = 2;
    break;
  case RGB:
    spp = 3;
    break;
  case PALETTE:
  case GS:
    spp = 1;
    break;
  default:
    fprintf(stderr,
            "Unknown pixel format.  This message probably shouldn't print.\n");
    return false;
    break;
  }

  for (uint32_t i = 0; i < hdr->height; i++) {
    uint32_t f_offset = i * bytes_per_row;
    uint32_t r_row_len = bytes_per_row - 1;
    uint32_t r_offset = i * r_row_len;
    uint8_t filter_type = out_data[f_offset];
    switch (filter_type) {
    case 0: // none-type filter (data just needs to be copied as is)
      for (uint32_t j = 0; j < r_row_len; j++) {
        raw_data[r_offset + j] = out_data[f_offset + j + 1];
      }
      break;
    case 1: // sub-type filter
            //(first pixel data is copied as is, then add the raw unfiltered
            // data from previous pixel to undo the sub filter)
      for (uint32_t j = 0; j < r_row_len; j++) {
        if (j < spp * bps) {
          raw_data[r_offset + j] = out_data[f_offset + j + 1];
          continue;
        }
        raw_data[r_offset + j] =
            out_data[f_offset + j + 1] + raw_data[r_offset + j - (spp * bps)];
      }
      break;
    case 2: // above-type filter
            // (first pixel data copied as is, then add raw unfiltered
            // data from previous row pixel to undo the filter)
      for (uint32_t j = 0; j < r_row_len; j++) {
        if (i == 0) {
          raw_data[r_offset + j] = out_data[f_offset + j + 1];
          continue;
        }
        raw_data[r_offset + j] =
            out_data[f_offset + j + 1] + raw_data[r_offset - r_row_len + j];
      }
      break;
    case 3:
      // printf("Average filter detected. TESTING.\n");
      // return false;
      // raw(x) = average + floor((raw(x-bpp)+prior(x))/2)
      // printf("bytes per pixel = %d\n", spp);
      for (uint32_t j = 0; j < r_row_len; j++) {
        if (i == 0 && j < spp * bps) {
          // first pixel -> raw(0) = average(0) + floor(0 + 0)
          raw_data[r_offset + j] = out_data[f_offset + j + 1];
          continue;
        }
        if (i == 0) {
          uint8_t average = raw_data[r_offset + j - (spp * bps)] >> 1;
          raw_data[r_offset + j] = out_data[f_offset + j + 1] + average;
          continue;
        }
        if (j < (spp)) {
          uint8_t average = raw_data[r_offset - r_row_len + j] >> 1;
          raw_data[r_offset + j] = out_data[f_offset + j + 1] + average;
          continue;
        }
        uint8_t average;
        uint16_t a = (uint16_t)raw_data[r_offset - r_row_len + j];
        uint16_t b = (uint16_t)raw_data[r_offset + j - (spp * bps)];
        average = (uint8_t)((a + b) >> 1);
        raw_data[r_offset + j] = out_data[f_offset + j + 1] + average;
      }
      break;
    case 4:
      for (uint32_t j = 0; j < r_row_len; j++) {
        if (i == 0 && j < spp * bps) {
          raw_data[r_offset + j] = out_data[f_offset + j + 1];
          continue;
        }
        if (i == 0) {
          // PaethPredictor(a, 0, 0) = a
          uint8_t a = raw_data[r_offset + j - (spp * bps)];
          raw_data[r_offset + j] = out_data[f_offset + j + 1] + a;
          continue;
        }
        if (j < spp * bps) {
          // PaethPredictor(0, b, 0) = b
          uint8_t b = raw_data[r_offset - r_row_len + j];
          raw_data[r_offset + j] = out_data[f_offset + j + 1] + b;
          continue;
        }
        // PaethPredictor(left, up, up-left)
        uint8_t left = raw_data[r_offset + j - (spp * bps)];
        uint8_t up = raw_data[r_offset - r_row_len + j];
        uint8_t up_left = raw_data[r_offset - r_row_len + j - (spp * bps)];
        uint8_t p = out_data[f_offset + j + 1];
        raw_data[r_offset + j] = p + PaethPredictor(left, up, up_left);
      }
      break;
    default:
      printf("Unsupported filter type for filter method 0.  This message "
             "shouldn't appear.\n");
      return false;
      break;
    }
  }
  return true;
}

PNG *decode_PNG(FILE *f) {
  if (get_file_size(f) < 45L) {
    invalid_png();
    printf("File size below minimum possible png size.\n");
    return NULL;
  }

  // Check the signature for a valid png file
  if (get_sig(f) != 1) {
    invalid_png();
    printf("Bad png signature.\n");
    return NULL;
  }
  CHUNK hdr_chunk = get_chunk(f);
  if (hdr_chunk.length != 13 || (strcmp(hdr_chunk.type, "") == 0) ||
      hdr_chunk.data == NULL) {
    printf("Invalid IHDR.\n");
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  PNG_IHDR *hdr_data = hdr_chunk.data;
  if (!verify_IHDR_data(hdr_data)) {
    free_chunk_data(&hdr_chunk);
    hdr_data = NULL;
    return NULL;
  }
  hdr_data->pal = NULL;

  // TODO: Add interlacing support
  if (hdr_data->interlace_method != 0) {
    printf("Interlaced png support not yet implemented.\n");
    free_chunk_data(&hdr_chunk);
    hdr_data = NULL;
    return NULL;
  }

  hdr_data->pixel_format = get_pixel_format(hdr_data);
  if (hdr_data->pixel_format == UNKNOWN) {
    printf("Invalid color depth/bit depth combination.\n");
    free_chunk_data(&hdr_chunk);
    hdr_data = NULL;
    return NULL;
  }

  CHUNK *chunks = (CHUNK *)calloc(1, sizeof(CHUNK));
  int i = 0;
  do {
    if (i != 0) {
      CHUNK *tmp;
      tmp = (CHUNK *)realloc(chunks, ((size_t)(1 + i) * sizeof(CHUNK)));
      if (!tmp) {
        if (i == 1) {
          free_chunk_data(chunks);
          free(chunks);
          chunks = NULL;
          hdr_data = NULL;
          free_chunk_data(&hdr_chunk);
          return NULL;
        }
        if (i != 1) {
          free_chunks(chunks, i);
          chunks = NULL;
          hdr_data = NULL;
          free_chunk_data(&hdr_chunk);
          return NULL;
        }
      }
      chunks = tmp;
    }
    chunks[i] = get_chunk(f);
    if (idat_start && (strcmp(chunks[i].type, "IDAT") != 0)) {
      idat_end = true;
    }
    if (idat_end && (strcmp(chunks[i].type, "IDAT") == 0)) {
      printf("Non-contiguous IDAT chunks detected.  Bad PNG\n");
      free_chunks(chunks, i);
      chunks = NULL;
      hdr_data = NULL;
      free_chunk_data(&hdr_chunk);
      return NULL;
    }
    if (strcmp(chunks[i].type, "IDAT") == 0) {
      idat_start = true;
    }
    i++;
  } while (strcmp(chunks[i - 1].type, "IEND") != 0);
  int num_chunks = i;

  // printf("Number of chunks: %d\n", i + 1);
  // printf("----------------\n");
  // print_chunk(&hdr_chunk);

  // for (i = 0; i < num_chunks; i++) {
  //   print_chunk(&chunks[i]);
  // }

  unsigned long num_bytes = 0L;
  for (i = 0; i < num_chunks; i++) {
    if (strcmp(chunks[i].type, "IDAT") == 0) {
      num_bytes += chunks[i].length;
    }
  }
  // printf("num_bytes before concatenation: %lu\n", num_bytes);
  unsigned char *compressed_data =
      (unsigned char *)malloc(num_bytes * sizeof(char));
  if (!compressed_data) {
    printf("Error allocating data.\n");
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  int offset = 0;
  for (i = 0; i < num_chunks; i++) {
    if (strcmp(chunks[i].type, "IDAT") != 0) {
      continue;
    }
    memcpy(compressed_data + offset, chunks[i].data, chunks[i].length);
    offset += chunks[i].length;
  }
  // printf("Offset after concatenating compressed data: %d\n", offset);
  size_t out_size = 0;
  size_t bytes_per_row = 0;
  uint8_t *out_data;
  if (decompress_pixels(compressed_data, num_bytes, hdr_data, &out_data,
                        &out_size, &bytes_per_row) != Z_OK) {
    printf("decompress_pixels failed\n");
    free(compressed_data);
    compressed_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  // printf("Bytes per row: %zu\n", bytes_per_row);
  // printf("Uncompressed data size: %zu\n", out_size);

  free(compressed_data);
  compressed_data = NULL;

  /**
   * Total number of bytes holding pixel data.
   **/
  size_t raw_size = out_size - hdr_data->height;
  uint8_t *raw_data = calloc(raw_size, 1);
  if (!raw_data) {
    printf("Error allocating raw_data.\n");
    if (out_data) {
      free(out_data);
    }
    out_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }

  if (!unfilter_data(raw_data, out_data, hdr_data, bytes_per_row)) {
    printf("Error unfiltering decompressed data.\n");
    free(raw_data);
    raw_data = NULL;
    if (out_data) {
      free(out_data);
      out_data = NULL;
    }
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  // printf("First filter byte: %02x\n", out_data[0]);
  free(out_data);
  out_data = NULL;

  if (hdr_data->bit_depth == 16) {
    raw_data = convert_16_to_8(raw_data, raw_size, hdr_data->pixel_format);
    raw_size /= 2;
  }

  // void *pixels = malloc(raw_size);
  // if (!pixels) {
  //   free(raw_data);
  //   raw_data = NULL;
  //   free_chunks(chunks, num_chunks);
  //   chunks = NULL;
  //   hdr_data = NULL;
  //   free_chunk_data(&hdr_chunk);
  //   return NULL;
  // }

  // if (!get_pixels(raw_data, hdr_data, pixels)) {
  //   if (pixels) {
  //     free(pixels);
  //     pixels = NULL;
  //   }
  //   free(raw_data);
  //   raw_data = NULL;
  //   free_chunks(chunks, num_chunks);
  //   chunks = NULL;
  //   hdr_data = NULL;
  //   free_chunk_data(&hdr_chunk);
  //   return NULL;
  // }
  uint8_t *pixels = NULL;
  // get_pixels2 will allocate data for pixels if necessary.
  if (!get_pixels2(raw_data, hdr_data, &pixels)) {
    // free(raw_data);
    raw_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  raw_data = NULL;
  if (hdr_data->bit_depth != 16) {
    apply_srgb(pixels, raw_size, hdr_data->pixel_format);
  }

  PNG *png = (PNG *)malloc(sizeof(PNG));
  if (!png) {
    // free(raw_data);
    // raw_data = NULL;
    if (pixels) {
      free(pixels);
    }
    pixels = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  png->header = hdr_data;
  png->pixels = pixels;
  png->bytes_per_row = bytes_per_row;

  // free(raw_data);
  // raw_data = NULL;
  free_chunks(chunks, num_chunks);
  chunks = NULL;
  return png;
}
