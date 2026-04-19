//
// Created by Ramon on 02/06/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <avif/avif.h>

#include "avif_writer.h"

int write_avif(const LoadedImage *img, avifRWData *out_data, const int speed, const int lossy_quality) {
    // RGB Image construction
    const avifRGBImage rgb = {
        .width = img->width,
        .height = img->height,
        .depth = 8,
        .format = AVIF_RGB_FORMAT_RGBA,
        .rowBytes = img->stride,
        .pixels = img->pixels,
        .maxThreads = 8
    };

    // YUV image construction
    avifImage *avif = avifImageCreate(img->width, img->height, 8,
                                      img->lossless ? AVIF_PIXEL_FORMAT_YUV444 : AVIF_PIXEL_FORMAT_YUV420);
    const avifResult yuv_result = avifImageRGBToYUV(avif, &rgb);
    if (yuv_result != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to convert RGB to YUV: %s\n", avifResultToString(yuv_result));
        avifImageDestroy(avif);
        return 1;
    }

    if (img->xmp_data != NULL) (void)avifImageSetMetadataXMP(avif, img->xmp_data, img->xmp_size);
    if (img->exif_data != NULL) (void)avifImageSetMetadataExif(avif, img->exif_data, img->exif_size);

    // Encoder setup
    avifEncoder *encoder = avifEncoderCreate();
    encoder->codecChoice = AVIF_CODEC_CHOICE_AUTO;
    encoder->maxThreads = 8;
    encoder->speed = speed;
    if (img->lossless) {
        encoder->quality = 100;
    } else {
        encoder->quality = lossy_quality;
    }

    // Encode
    const avifResult result = avifEncoderWrite(encoder, avif, out_data);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to encode AVIF: %s\n", avifResultToString(result));
        avifEncoderDestroy(encoder);
        avifImageDestroy(avif);
        return 1;
    }

    avifEncoderDestroy(encoder);
    avifImageDestroy(avif);

    return 0;
}
