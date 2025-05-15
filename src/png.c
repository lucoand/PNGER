#include "png.h"

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

typedef struct {
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t a;
} PIXEL16;

uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c);

PIXEL *allocate_PIXELs(PNG_IHDR *hdr) {
  size_t num_pixels = hdr->height * hdr->width;
  PIXEL *pixels = calloc(num_pixels, sizeof(PIXEL));
  if (!pixels) {
    printf("Allocation error: pixels.\n");
    return NULL;
  }
  return pixels;
}

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
    printf("Invalid filter method.\n");
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
                       PixelFormat pixel_format) {
  int samples_per_pixel;
  switch (pixel_format) {
  case GS:
    samples_per_pixel = 1;
    break;
  case RGB:
    samples_per_pixel = 3;
    break;
  case PALETTE:
    samples_per_pixel = 1;
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
  size_t buffer_size;
  if (pixel_format == PALETTE) {
    // 1 byte per pixel + 1 byte per scanline
    buffer_size = height * (width + 1);
    return buffer_size;
  }
  size_t bytes_per_row;
  switch (bit_depth) {
  case 16:
    bytes_per_row = width * samples_per_pixel * 2 + 1;
    break;
  case 8:
    bytes_per_row = width * samples_per_pixel + 1;
    break;
  case 4:
    bytes_per_row = (size_t)ceil((double)width / 2) + 1;
    break;
  case 2:
    bytes_per_row = (size_t)ceil((double)width / 4) + 1;
    break;
  case 1:
    bytes_per_row = (size_t)ceil((double)width / 8) + 1;
    break;
  default:
    return 0;
    break;
  }

  buffer_size = height * bytes_per_row;
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
                      size_t *out_size) {
  if (!compressed_data || !hdr || !out_data || !out_size) {
    printf("Null pointer passed to decompress pixels.\n");
    return -1;
  }
  size_t buffer_size = get_buffer_size(hdr->height, hdr->width, hdr->bit_depth,
                                       hdr->pixel_format);
  // printf("zlib version: %s\n", zlibVersion());
  // printf("First 4 bytes of compressed data: %02x %02x %02x %02x\n",
  //        compressed_data[0], compressed_data[1], compressed_data[2],
  //        compressed_data[3]);
  *out_data = malloc(buffer_size);
  if (!*out_data) {
    fprintf(stderr, "malloc failed for buffer size %zu\n", buffer_size);
    return -1;
  }

  // fprintf(stderr, "Before uncompress:\n");
  // fprintf(stderr, "  compressed_data: %p\n", (void *)compressed_data);
  // fprintf(stderr, "  compressed_size: %zu\n", compressed_size);
  // fprintf(stderr, "  out_data: %p\n", (void *)*out_data);
  // fprintf(stderr, "  buffer_size: %zu\n", buffer_size);
  //
  // fprintf(stderr, "Calling uncompress...\n");
  // fflush(stderr);
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

int get_RGBA_pixels(uint8_t **out_data, size_t out_size, PNG_IHDR *hdr,
                    PIXEL *pixels) {
  // four bytes per pixel
  uint8_t *data = *out_data;
  size_t offset = hdr->width * 4 + 1;
  for (int i = 0; i < hdr->height; i++) {
    uint8_t filter_type = data[i * offset];
    switch (filter_type) {
    case 0:
      for (int j = 0; j < hdr->width; j++) {
        pixels[hdr->width * i + j].r = data[offset * i + j * 4 + 1];
        pixels[hdr->width * i + j].g = data[offset * i + j * 4 + 1 + 1];
        pixels[hdr->width * i + j].b = data[offset * i + j * 4 + 1 + 2];
        pixels[hdr->width * i + j].a = data[offset * i + j * 4 + 1 + 3];
      }
      break;
    case 1:
      for (int j = 0; j < hdr->width; j++) {
        if (j == 0) {
          pixels[hdr->width * i + j].r = data[offset * i + j * 4 + 1];
          pixels[hdr->width * i + j].g = data[offset * i + j * 4 + 1 + 1];
          pixels[hdr->width * i + j].b = data[offset * i + j * 4 + 1 + 2];
          pixels[hdr->width * i + j].a = data[offset * i + j * 4 + 1 + 3];
          continue;
        }
        pixels[hdr->width * i + j].r =
            data[offset * i + j * 4 + 1] + pixels[hdr->width * i + j - 1].r;
        pixels[hdr->width * i + j].g =
            data[offset * i + j * 4 + 1 + 1] + pixels[hdr->width * i + j - 1].g;
        pixels[hdr->width * i + j].b =
            data[offset * i + j * 4 + 1 + 2] + pixels[hdr->width * i + j - 1].b;
        pixels[hdr->width * i + j].a =
            data[offset * i + j * 4 + 1 + 3] + pixels[hdr->width * i + j - 1].a;
      }
      break;
    case 2:
      for (int j = 0; j < hdr->width; j++) {
        if (i == 0) {
          pixels[hdr->width * i + j].r = data[offset * i + j * 4 + 1];
          pixels[hdr->width * i + j].g = data[offset * i + j * 4 + 1 + 1];
          pixels[hdr->width * i + j].b = data[offset * i + j * 4 + 1 + 2];
          pixels[hdr->width * i + j].a = data[offset * i + j * 4 + 1 + 3];
          continue;
        }
        pixels[hdr->width * i + j].r =
            data[offset * i + j * 4 + 1] + pixels[hdr->width * (i - 1) + j].r;
        pixels[hdr->width * i + j].g = data[offset * i + j * 4 + 1 + 1] +
                                       pixels[hdr->width * (i - 1) + j].g;
        pixels[hdr->width * i + j].b = data[offset * i + j * 4 + 1 + 2] +
                                       pixels[hdr->width * (i - 1) + j].b;
        pixels[hdr->width * i + j].a = data[offset * i + j * 4 + 1 + 3] +
                                       pixels[hdr->width * (i - 1) + j].a;
      }
      break;
    case 4:
      for (int j = 0; j < hdr->width; j++) {
        if (i == 0 && j == 0) {
          pixels[0].r = data[1];
          pixels[0].g = data[2];
          pixels[0].b = data[3];
          pixels[0].a = data[4];
          continue;
        }
        if (i == 0) {
          uint8_t pr = PaethPredictor(pixels[j - 1].r, 0, 0);
          uint8_t pg = PaethPredictor(pixels[j - 1].g, 0, 0);
          uint8_t pb = PaethPredictor(pixels[j - 1].b, 0, 0);
          uint8_t pa = PaethPredictor(pixels[j - 1].a, 0, 0);
          pixels[j].r = data[j * 4 + 1] + pr;
          pixels[j].g = data[j * 4 + 1 + 1] + pg;
          pixels[j].b = data[j * 4 + 1 + 2] + pb;
          pixels[j].a = data[j * 4 + 1 + 3] + pa;
          continue;
        }
        if (j == 0) {
          uint8_t pr = PaethPredictor(0, pixels[hdr->width * (i - 1)].r, 0);
          uint8_t pg = PaethPredictor(0, pixels[hdr->width * (i - 1)].g, 0);
          uint8_t pb = PaethPredictor(0, pixels[hdr->width * (i - 1)].b, 0);
          uint8_t pa = PaethPredictor(0, pixels[hdr->width * (i - 1)].a, 0);
          pixels[hdr->width * i].r = data[offset * i + 1] + pr;
          pixels[hdr->width * i].g = data[offset * i + 1 + 1] + pg;
          pixels[hdr->width * i].b = data[offset * i + 1 + 2] + pb;
          pixels[hdr->width * i].a = data[offset * i + 1 + 3] + pa;
          continue;
        }
        uint8_t pr = PaethPredictor(pixels[hdr->width * i + j - 1].r,
                                    pixels[hdr->width * (i - 1) + j].r,
                                    pixels[hdr->width * (i - 1) + j - 1].r);
        uint8_t pg = PaethPredictor(pixels[hdr->width * i + j - 1].g,
                                    pixels[hdr->width * (i - 1) + j].g,
                                    pixels[hdr->width * (i - 1) + j - 1].g);
        uint8_t pb = PaethPredictor(pixels[hdr->width * i + j - 1].b,
                                    pixels[hdr->width * (i - 1) + j].b,
                                    pixels[hdr->width * (i - 1) + j - 1].b);
        uint8_t pa = PaethPredictor(pixels[hdr->width * i + j - 1].a,
                                    pixels[hdr->width * (i - 1) + j].a,
                                    pixels[hdr->width * (i - 1) + j - 1].a);
        pixels[hdr->width * i + j].r = data[offset * i + j * 4 + 1] + pr;
        pixels[hdr->width * i + j].g = data[offset * i + j * 4 + 1 + 1] + pg;
        pixels[hdr->width * i + j].b = data[offset * i + j * 4 + 1 + 2] + pb;
        pixels[hdr->width * i + j].a = data[offset * i + j * 4 + 1 + 3] + pa;
      }
      break;
    default:
      printf("Filter type not yet implemented: %d\n", filter_type);
      return 1;
      break;
    }
  }
  return 0;
}

int get_pixels(uint8_t **out_data, size_t out_size, PNG_IHDR *hdr,
               PIXEL *pixels) {
  if (!out_data || !hdr || !pixels) {
    fprintf(stderr, "Null pointer passed into get_pixels.\n");
    return 1;
  }
  switch (hdr->pixel_format) {
  case RGBA:
    if (hdr->bit_depth == 8) {
      if (get_RGBA_pixels(out_data, out_size, hdr, pixels) == 0) {
        return 0;
      } else {
        printf("Couldn't get pixels.\n");
        return 1;
      }
    }
    if (hdr->bit_depth == 16) {
      printf("Bit depth 16 RGBA not yet implemented.\n");
      return 1;
    }
    printf("Unsupported bit depth for RGBA.  This message shouldn't appear.\n");
    return 1;
    break;
  default:
    printf("Pixel format not yet implemented.\n");
    return 1;
    break;
  }
}

void print_pixel(PIXEL p) {
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

PNG *decode_PNG(FILE *f) {
  if (get_file_size(f) < (long)45) {
    invalid_png();
    return NULL;
  }

  // Check the signature for a valid png file
  if (get_sig(f) != 1) {
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
  uint8_t *out_data;
  if (decompress_pixels(compressed_data, num_bytes, hdr_data, &out_data,
                        &out_size) != Z_OK) {
    printf("decompress_pixels failed\n");
    free(compressed_data);
    compressed_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }

  // printf("Uncompressed data size: %zu\n", out_size);

  free(compressed_data);
  compressed_data = NULL;

  // printf("First filter byte: %02x\n", out_data[0]);

  PIXEL *pixels = allocate_PIXELs(hdr_data);
  if (!pixels) {
    free(out_data);
    out_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    return NULL;
  }
  // printf("out_data before get_pixels: %p\n", out_data);
  int pixel_result = get_pixels(&out_data, out_size, hdr_data, pixels);
  if (pixel_result == 1) {
    if (pixels) {
      free(pixels);
    }
    pixels = NULL;
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

  PNG *png = (PNG *)malloc(sizeof(PNG));
  if (!png) {
    if (pixels) {
      free(pixels);
    }
    pixels = NULL;
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
  png->header = hdr_data;
  png->pixels = pixels;

  if (out_data) {
    free(out_data);
  }
  out_data = NULL;
  free_chunks(chunks, num_chunks);
  chunks = NULL;
  return png;
}

// int main(int argc, char **argv) {
//   if (argc < 2) {
//     printf("Too few arguments\n");
//     return 1;
//   }
//   if (argc > 2) {
//     printf("Too many arguments\n");
//     return 1;
//   }
//   // printf("%s\n", argv[1]);
//   FILE *f = fopen(argv[1], "rb");
//   if (!f) {
//     perror("fopen");
//     return 1;
//   }
//
//   PNG *png = decode_PNG(f);
//
//   print_pixel(png->pixels[0]);
//   printf("Width: %d\nHeight: %d\n", png->header->width, png->header->height);
//
//   free_PNG(png);
//   fclose(f);
//   return 0;
// }

int dummy(int argc, char **argv) {
  if (argc < 2) {
    printf("Too few arguments\n");
    return 1;
  }
  if (argc > 2) {
    printf("Too many arguments\n");
    return 1;
  }
  // printf("%s\n", argv[1]);
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }
  // Apparently the minimum possible size for a png is 45 bytes
  // so it makes sense to me to check for that.
  if (get_file_size(f) < (long)45) {
    invalid_png();
    fclose(f);
    return 1;
  }

  // Check the signature for a valid png file
  if (get_sig(f) != 1) {
    invalid_png();
    fclose(f);
    return 1;
  }
  printf("File %s has a valid png signature!\n", argv[1]);
  CHUNK hdr_chunk = get_chunk(f);
  if (hdr_chunk.length != 13 || (strcmp(hdr_chunk.type, "") == 0) ||
      hdr_chunk.data == NULL) {
    printf("Invalid IHDR.\n");
    free_chunk_data(&hdr_chunk);
    fclose(f);
    return 1;
  }
  PNG_IHDR *hdr_data = hdr_chunk.data;
  if (!verify_IHDR_data(hdr_data)) {
    free_chunk_data(&hdr_chunk);
    hdr_data = NULL;
    fclose(f);
    return 1;
  }
  hdr_data->pixel_format = get_pixel_format(hdr_data);
  if (hdr_data->pixel_format == UNKNOWN) {
    printf("Invalid color depth/bit depth combination.\n");
    free_chunk_data(&hdr_chunk);
    hdr_data = NULL;
    fclose(f);
    return 1;
  }

  CHUNK *chunks = (CHUNK *)calloc(1, sizeof(CHUNK));
  // bool first_chunk = true;
  // chunks[0] = get_chunk(f);
  // print_chunk(&chunks[0]);
  //
  // chunks[1] = get_chunk(f);
  // print_chunk(&chunks[1]);
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
          fclose(f);
          return 1;
        }
        if (i != 1) {
          free_chunks(chunks, i);
          chunks = NULL;
          hdr_data = NULL;
          free_chunk_data(&hdr_chunk);
          fclose(f);
          return 1;
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
      fclose(f);
      return 1;
    }
    if (strcmp(chunks[i].type, "IDAT") == 0) {
      idat_start = true;
    }
    i++;
  } while (strcmp(chunks[i - 1].type, "IEND") != 0);
  int num_chunks = i;

  printf("Number of chunks: %d\n", i + 1);
  printf("----------------\n");
  print_chunk(&hdr_chunk);

  for (i = 0; i < num_chunks; i++) {
    print_chunk(&chunks[i]);
  }

  unsigned long num_bytes = 0L;
  for (i = 0; i < num_chunks; i++) {
    if (strcmp(chunks[i].type, "IDAT") == 0) {
      num_bytes += chunks[i].length;
    }
  }
  printf("num_bytes before concatenation: %lu\n", num_bytes);
  unsigned char *compressed_data =
      (unsigned char *)malloc(num_bytes * sizeof(char));
  if (!compressed_data) {
    printf("Error allocating data.\n");
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    fclose(f);
    return 1;
  }
  int offset = 0;
  for (i = 0; i < num_chunks; i++) {
    if (strcmp(chunks[i].type, "IDAT") != 0) {
      continue;
    }
    memcpy(compressed_data + offset, chunks[i].data, chunks[i].length);
    offset += chunks[i].length;
  }
  printf("Offset after concatenating compressed data: %d\n", offset);
  // unsigned long expected_num_bytes = 0L;
  // for (i = 0; i < num_chunks; i++) {
  //   if (strcmp(chunks[i].type, "IDAT") == 0) {
  //     expected_num_bytes += chunks[i].length;
  //   }
  // }

  // printf("Expected compressed data length? %s\n",
  //        expected_num_bytes == num_bytes ? "true" : "false");
  // printf("Expected compressed data length: %lu\n", expected_num_bytes);
  // printf("Actual compressed data length: %lu\n", num_bytes);

  // printf("First two bytes of compressed stream: %x %x\n", compressed_data[0],
  //        compressed_data[1]);
  size_t out_size = 0;
  uint8_t *out_data;
  if (decompress_pixels(compressed_data, num_bytes, hdr_data, &out_data,
                        &out_size) != Z_OK) {
    printf("decompress_pixels failed\n");
    free(compressed_data);
    compressed_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    fclose(f);
    return 1;
  }

  printf("Uncompressed data size: %zu\n", out_size);

  free(compressed_data);
  compressed_data = NULL;

  printf("First filter byte: %02x\n", out_data[0]);

  PIXEL *pixels = allocate_PIXELs(hdr_data);
  if (!pixels) {
    free(out_data);
    out_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    fclose(f);
    return 1;
  }
  printf("out_data before get_pixels: %p\n", out_data);
  int pixel_result = get_pixels(&out_data, out_size, hdr_data, pixels);
  if (pixel_result == 1) {
    if (pixels) {
      free(pixels);
    }
    if (out_data) {
      free(out_data);
    }
    out_data = NULL;
    free_chunks(chunks, num_chunks);
    chunks = NULL;
    hdr_data = NULL;
    free_chunk_data(&hdr_chunk);
    fclose(f);
    return 1;
  }
  print_pixel(pixels[0]);

  if (pixels) {
    free(pixels);
  }
  if (out_data) {
    free(out_data);
  }
  out_data = NULL;
  free_chunks(chunks, num_chunks);
  chunks = NULL;
  hdr_data = NULL;
  free_chunk_data(&hdr_chunk);
  fclose(f);
  printf("All clean!\n");
  return 0;
}
