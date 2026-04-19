//
// Created by Ramon on 02/06/2025.
//

#include "avif_convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avif/avif.h>

#include "input/loaders.h"
#include "output/avif_writer.h"

AVIF_CONVERT_API int convert_to_avif(
    const uint8_t *input_data,
    const size_t input_size,
    uint8_t **output_data,
    size_t *output_size,
    const int speed,
    const int lossy_quality
) {
    LoadedImage img;
    switch (detect_input_format(input_data, input_size)) {
        case FORMAT_AVIF:
            // Simply copy the input into the output, we don't need to rewrite an existing AVIF file
            *output_data = malloc(input_size);
            if (!*output_data) return 1;
            *output_size = input_size;
            memcpy(*output_data, input_data, input_size);
            return 0;
        case FORMAT_PNG:
            if (load_png(input_data, input_size, &img) != 0) {
                return 1;
            }
            break;
        case FORMAT_JPEG:
            if (load_jpg(input_data, input_size, &img) != 0) {
                return 1;
            }
            break;
        case FORMAT_WEBP:
            if (load_webp(input_data, input_size, &img) != 0) {
                return 1;
            }
            break;
        case FORMAT_HEIF:
            if (load_heif(input_data, input_size, &img) != 0) {
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Unsupported or unrecognized input format\n");
            return 1;
    }

    if (!img.pixels) {
        fprintf(stderr, "Failed to load image data\n");
        free_loaded_image(&img);
        return 1;
    }

    avifRWData output_avif = AVIF_DATA_EMPTY;
    const int result = write_avif(&img, &output_avif, speed, lossy_quality);
    free_loaded_image(&img);

    *output_data = malloc(output_avif.size);
    if (*output_data == NULL) {
        avifRWDataFree(&output_avif);
        *output_size = 0;
        return 1;
    }
    memcpy(*output_data, output_avif.data, output_avif.size);
    *output_size = output_avif.size;

    avifRWDataFree(&output_avif);

    return result;
}

AVIF_CONVERT_API int is_convertible(
    const uint8_t *input_data,
    const size_t input_size
) {
    switch (detect_input_format(input_data, input_size)) {
        case FORMAT_PNG:
        case FORMAT_JPEG:
        case FORMAT_WEBP:
        case FORMAT_HEIF:
            return 1;
        case FORMAT_AVIF:
        default:
            return 0;
    }
}
