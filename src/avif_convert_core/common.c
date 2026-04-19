//
// Created by Ramon on 02/06/2025.
//
#include "common.h"

#include <avif/avif.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

InputFormat detect_input_format(const uint8_t *buf, size_t len) {
    if (len >= 8 && memcmp(buf, "\x89PNG\r\n\x1a\n", 8) == 0)
        return FORMAT_PNG;

    if (len >= 3 && memcmp(buf, "\xff\xd8\xff", 3) == 0)
        return FORMAT_JPEG;

    if (len >= 12 && memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WEBP", 4) == 0)
        return FORMAT_WEBP;

    if (len >= 6 &&
        memcmp(buf, "GIF", 3) == 0 &&
        (memcmp(buf + 3, "87a", 3) == 0 || memcmp(buf + 3, "89a", 3) == 0))
        return FORMAT_GIF;

    if (len >= 12 && memcmp(buf + 4, "ftyp", 4) == 0) {
        const char *brand = (const char *) (buf + 8);
        if (memcmp(brand, "heic", 4) == 0 || memcmp(brand, "heix", 4) == 0 ||
            memcmp(brand, "hevc", 4) == 0 || memcmp(brand, "hevx", 4) == 0 ||
            memcmp(brand, "mif1", 4) == 0 || memcmp(brand, "msf1", 4) == 0)
            return FORMAT_HEIF;

        if (memcmp(brand, "avif", 4) == 0 || memcmp(brand, "avis", 4) == 0)
            return FORMAT_AVIF;
    }

    return FORMAT_UNKNOWN;
}

LoadedImage construct_image(
    uint8_t *pixels,
    const unsigned int width,
    const unsigned int height,
    const unsigned int stride,
    const bool lossless
) {
    LoadedImage img = {0};

    img.pixels = pixels;
    img.width = width;
    img.height = height;
    img.stride = stride;
    img.lossless = lossless;

    return img;
}

void free_loaded_image(LoadedImage *img) {
    if (!img) return;
    if (img->pixels != NULL) free(img->pixels);
    if (img->xmp_data != NULL) free(img->xmp_data);
    if (img->exif_data != NULL) free(img->exif_data);
}

size_t remove_control_chars(char *data, const size_t len) {
    size_t write = 0;

    for (size_t read = 0; read < len; read++) {
        const char c = data[read];
        if (!(c <= 0x1F || (c >= 0x7F && c <= 0x9F))) {
            data[write++] = c;
        }
    }

    return write; // new length
}

static int hexval(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1; // Invalid
}

unsigned char *hex_decode(const char *hex, size_t hex_len, size_t *out_len) {
    if (hex_len % 2 != 0) return NULL;

    const size_t blen = hex_len / 2;
    unsigned char *out = malloc(blen);
    if (!out) return NULL;

    for (size_t i = 0; i < blen; i++) {
        const int hi = hexval(hex[2 * i]);
        const int lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            free(out);
            return NULL;
        }
        out[i] = hi << 4 | lo;
    }

    if (out_len) *out_len = blen;
    return out;
}

size_t find_terminated_length(const uint8_t *haystack, const size_t haystack_len, const uint8_t *needle,
                              const size_t needle_len) {
    if (needle_len == 0 || haystack_len < needle_len) return haystack_len;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return i;
        }
    }
    return haystack_len;
}
