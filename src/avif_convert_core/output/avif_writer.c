//
// Created by Ramon on 02/06/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <avif/avif.h>

#include "avif_writer.h"

static avifImage *frame_to_avif(const LoadedImage *img, unsigned int frame_idx) {
    const LoadedFrame *frame = &img->frames[frame_idx];
    const avifRGBImage rgb = {
        .width = img->width,
        .height = img->height,
        .depth = 8,
        .format = AVIF_RGB_FORMAT_RGBA,
        .rowBytes = img->stride,
        .pixels = frame->pixels,
        .maxThreads = 8
    };
    avifImage *avif = avifImageCreate(img->width, img->height, 8,
                                      img->lossless ? AVIF_PIXEL_FORMAT_YUV444 : AVIF_PIXEL_FORMAT_YUV420);
    const avifResult r = avifImageRGBToYUV(avif, &rgb);
    if (r != AVIF_RESULT_OK) {
        fprintf(stderr, "Failed to convert RGB to YUV: %s\n", avifResultToString(r));
        avifImageDestroy(avif);
        return NULL;
    }
    return avif;
}

int write_avif(const LoadedImage *img, avifRWData *out_data, const int speed, const int lossy_quality) {
    avifEncoder *encoder = avifEncoderCreate();
    encoder->codecChoice = AVIF_CODEC_CHOICE_AUTO;
    encoder->maxThreads = 8;
    encoder->speed = speed;
    encoder->quality = img->lossless ? 100 : lossy_quality;

    int result = 0;

    if (img->frame_count == 1) {
        avifImage *avif = frame_to_avif(img, 0);
        if (!avif) { avifEncoderDestroy(encoder); return 1; }

        if (img->xmp_data)  (void)avifImageSetMetadataXMP(avif, img->xmp_data, img->xmp_size);
        if (img->exif_data) (void)avifImageSetMetadataExif(avif, img->exif_data, img->exif_size);

        const avifResult r = avifEncoderWrite(encoder, avif, out_data);
        if (r != AVIF_RESULT_OK) {
            fprintf(stderr, "Failed to encode AVIF: %s\n", avifResultToString(r));
            result = 1;
        }
        avifImageDestroy(avif);
    } else {
        // GIF delay is in centiseconds; use timescale=100 so duration ticks map 1:1
        encoder->timescale = 100;

        for (unsigned int i = 0; i < img->frame_count; i++) {
            avifImage *avif = frame_to_avif(img, i);
            if (!avif) { result = 1; break; }

            if (i == 0) {
                if (img->xmp_data)  (void)avifImageSetMetadataXMP(avif, img->xmp_data, img->xmp_size);
                if (img->exif_data) (void)avifImageSetMetadataExif(avif, img->exif_data, img->exif_size);
            }

            // duration_ms → centiseconds (round up to at least 1 tick)
            const uint64_t duration_cs = img->frames[i].duration_ms / 10;
            const avifResult r = avifEncoderAddImage(encoder, avif,
                                                     duration_cs > 0 ? duration_cs : 1,
                                                     AVIF_ADD_IMAGE_FLAG_NONE);
            avifImageDestroy(avif);
            if (r != AVIF_RESULT_OK) {
                fprintf(stderr, "Failed to add frame %u: %s\n", i, avifResultToString(r));
                result = 1;
                break;
            }
        }

        if (result == 0) {
            const avifResult r = avifEncoderFinish(encoder, out_data);
            if (r != AVIF_RESULT_OK) {
                fprintf(stderr, "Failed to finish AVIF: %s\n", avifResultToString(r));
                result = 1;
            }
        }
    }

    avifEncoderDestroy(encoder);
    return result;
}
