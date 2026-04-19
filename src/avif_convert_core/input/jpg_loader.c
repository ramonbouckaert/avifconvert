//
// Created by Ramon on 02/06/2025.
//
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <string.h>

#include "loaders.h"

#define EXIF_MARKER 0xE1
#define XMP_IDENTIFIER "http://ns.adobe.com/xap/1.0/"

int load_jpg(const uint8_t *data, const size_t size, LoadedImage *out_image) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, data, (unsigned long) size);
    jpeg_save_markers(&cinfo, EXIF_MARKER, 0xFFFF);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    unsigned int width = cinfo.output_width;
    unsigned int height = cinfo.output_height;
    int channels = cinfo.output_components; // usually 3 (RGB)

    // Force RGB output
    if (channels != 3) {
        fprintf(stderr, "Unsupported number of channels: %d\n", channels);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }

    unsigned int row_stride = width * 4;
    uint8_t *buffer = malloc(height * row_stride);
    if (!buffer) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }

    JSAMPARRAY row = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, width * channels, 1);

    for (int y = 0; y < height; y++) {
        jpeg_read_scanlines(&cinfo, row, 1);
        for (int x = 0; x < width; x++) {
            buffer[y * row_stride + x * 4 + 0] = row[0][x * 3 + 0]; // R
            buffer[y * row_stride + x * 4 + 1] = row[0][x * 3 + 1]; // G
            buffer[y * row_stride + x * 4 + 2] = row[0][x * 3 + 2]; // B
            buffer[y * row_stride + x * 4 + 3] = 255; // A
        }
    }

    *out_image = construct_image(
        buffer,
        width,
        height,
        row_stride,
        false // JPEG is lossy by default
    );

    // Handle metadata
    jpeg_saved_marker_ptr marker = cinfo.marker_list;
    while (marker) {
        if (marker->marker == EXIF_MARKER) {
            if (
                marker->data_length > 6 && memcmp(marker->data, "Exif\0\0", 6) == 0
            ) {
                // Scan for the TIFF header magic — use everything from that point regardless
                // of whatever prefix bytes precede it (typically "Exif\0\0").
                static const uint8_t tiff_le[] = {0x49, 0x49, 0x2A, 0x00};
                static const uint8_t tiff_be[] = {0x4D, 0x4D, 0x00, 0x2A};
                size_t offset = marker->data_length; // sentinel: not found
                for (size_t j = 0; j + 4 <= marker->data_length; j++) {
                    if (memcmp(marker->data + j, tiff_le, 4) == 0 ||
                        memcmp(marker->data + j, tiff_be, 4) == 0) {
                        offset = j;
                        break;
                    }
                }
                if (offset < marker->data_length) {
                    const size_t exif_size = marker->data_length - offset;
                    unsigned char *exif = malloc(exif_size);
                    if (exif) {
                        memcpy(exif, marker->data + offset, exif_size);
                        out_image->exif_data = exif;
                        out_image->exif_size = exif_size;
                    }
                }
            } else if (
                marker->data_length > strlen(XMP_IDENTIFIER) &&
                memcmp(marker->data, XMP_IDENTIFIER, strlen(XMP_IDENTIFIER)) == 0
            ) {
                // Skip the APP1 identifier string and its null terminator — store raw XMP
                const size_t offset = strlen(XMP_IDENTIFIER) + 1;
                const size_t xmp_size = marker->data_length - offset;
                unsigned char *xmp = malloc(xmp_size);
                if (xmp) {
                    memcpy(xmp, marker->data + offset, xmp_size);
                    out_image->xmp_data = xmp;
                    out_image->xmp_size = xmp_size;
                }
            }
        }
        marker = marker->next;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return 0;
}
