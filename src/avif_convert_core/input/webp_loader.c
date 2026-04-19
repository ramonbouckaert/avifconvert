//
// Created by Ramon on 02/06/2025.
//
#include <webp/decode.h>
#include <webp/demux.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loaders.h"

// Utility: Read 4-character chunk type
static int is_chunk(const uint8_t *data, const char *tag) {
    return memcmp(data, tag, 4) == 0;
}

int detect_webp_lossless(const uint8_t *data, const size_t size) {
    if (size < 16 || !is_chunk(data, "RIFF") || !is_chunk(data + 8, "WEBP")) {
        return 1; // Not a valid WebP
    }

    const uint8_t *ptr = data + 12; // Skip RIFF header

    while (ptr < data + size - 8) {
        if (is_chunk(ptr, "VP8L")) return 1; // Lossless
        if (is_chunk(ptr, "VP8 ")) return 0; // Lossy
        if (is_chunk(ptr, "VP8X")) {
            // Extended format — keep going
            const uint32_t chunk_size = ptr[4] | ptr[5] << 8 | ptr[6] << 16 | ptr[7] << 24;
            ptr += 8 + ((chunk_size + 1) & ~1); // 8 bytes header + aligned size
            continue;
        }

        // Unknown chunk — skip
        uint32_t chunk_size = ptr[4] | ptr[5] << 8 | ptr[6] << 16 | ptr[7] << 24;
        ptr += 8 + ((chunk_size + 1) & ~1);
    }

    return -1; // Could not determine
}

int load_webp(const uint8_t *data, const size_t size, LoadedImage *out_image) {
    int width = 0;
    int height = 0;

    // Detect whether it's a lossless or lossy webp
    const int lossless = detect_webp_lossless(data, size);

    // Get dimensions without decoding
    if (!WebPGetInfo(data, size, &width, &height)) {
        fprintf(stderr, "WebP decode failed\n");
        return 1;
    }

    unsigned char *rgba = malloc((size_t)width * height * 4);
    if (!rgba) {
        fprintf(stderr, "Out of memory for WebP image\n");
        return 1;
    }

    if (!WebPDecodeRGBAInto(data, size, rgba, (size_t)width * height * 4, width * 4)) {
        fprintf(stderr, "WebP decode failed\n");
        free(rgba);
        return 1;
    }

    *out_image = construct_image(
        rgba,
        width,
        height,
        width * 4, // 4 bytes per pixel (RGBA)
        lossless == 1 // 1 = lossless, 0 = lossy
    );

    // Handle metadata
    const WebPData webp_data = {.bytes = data, .size = size};
    WebPDemuxer *demux = WebPDemux(&webp_data);
    if (!demux) {
        fprintf(stderr, "WebP demux failed\n");
        return 1;
    }
    static const uint8_t tiff_le[] = {0x49, 0x49, 0x2A, 0x00};
    static const uint8_t tiff_be[] = {0x4D, 0x4D, 0x00, 0x2A};
    WebPChunkIterator chunk_iter;
    if (WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter)) {
        const uint8_t *src = chunk_iter.chunk.bytes;
        const size_t src_size = chunk_iter.chunk.size;
        for (size_t j = 0; j + 4 <= src_size; j++) {
            if (memcmp(src + j, tiff_le, 4) == 0 || memcmp(src + j, tiff_be, 4) == 0) {
                const size_t exif_size = src_size - j;
                unsigned char *exif = malloc(exif_size);
                if (exif) {
                    memcpy(exif, src + j, exif_size);
                    out_image->exif_data = exif;
                    out_image->exif_size = exif_size;
                }
                break;
            }
        }
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }
    if (WebPDemuxGetChunk(demux, "XMP ", 1, &chunk_iter)) {
        unsigned char *xmp = malloc(chunk_iter.chunk.size);
        if (xmp) {
            memcpy(xmp, chunk_iter.chunk.bytes, chunk_iter.chunk.size);
            out_image->xmp_data = xmp;
            out_image->xmp_size = chunk_iter.chunk.size;
        }
        WebPDemuxReleaseChunkIterator(&chunk_iter);
    }
    WebPDemuxDelete(demux);

    return 0;
}
