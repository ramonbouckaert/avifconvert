//
// Created by Ramon on 02/06/2025.
//

#ifndef LOADERS_H
#define LOADERS_H
#include <stdint.h>

#include "../common.h"
int load_gif(const uint8_t *data, size_t size, LoadedImage *out_image);
int load_heif(const uint8_t *data, size_t size, LoadedImage *out_image);
int load_jpg(const uint8_t *data, size_t size, LoadedImage *out_image);
int load_png(const uint8_t *data, size_t size, LoadedImage *out_image);
int load_webp(const uint8_t *data, size_t size, LoadedImage *out_image);
#endif //LOADERS_H
