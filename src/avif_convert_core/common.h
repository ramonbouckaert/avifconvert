//
// Created by Ramon on 02/06/2025.
//

#ifndef COMMON_H
#define COMMON_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    FORMAT_UNKNOWN,
    FORMAT_AVIF,
    FORMAT_PNG,
    FORMAT_JPEG,
    FORMAT_WEBP,
    FORMAT_HEIF,
    FORMAT_GIF
} InputFormat;

InputFormat detect_input_format(const uint8_t *buf, size_t len);

typedef struct {
    uint8_t *pixels;       // RGBA, owned
    unsigned int duration_ms;
} LoadedFrame;

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    bool lossless;

    LoadedFrame *frames;
    unsigned int frame_count;

    // Optional metadata
    unsigned char *exif_data;
    size_t exif_size;

    unsigned char *xmp_data;
    size_t xmp_size;
} LoadedImage;

LoadedImage construct_image(
    uint8_t *pixels,
    unsigned int width,
    unsigned int height,
    unsigned int stride,
    bool lossless
);

void free_loaded_image(LoadedImage *img);

size_t remove_control_chars(char *data, size_t len);

unsigned char *hex_decode(const char *hex, size_t hex_len, size_t *out_len);

size_t find_terminated_length(const uint8_t *haystack, size_t haystack_len, const uint8_t *needle, size_t needle_len);

#endif //COMMON_H
