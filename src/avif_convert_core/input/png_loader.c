//
// Created by Ramon on 02/06/2025.
//

#include <spng.h>
#include <stdio.h>
#include <string.h>

#include "loaders.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

#define EXIF_KEYWORD "Raw profile type exif"
#define XMP_KEYWORD "XML:com.adobe.xmp"

void decode_exif_from_hex_string_png(
    char *in_data,
    const size_t *in_len,
    unsigned char **out_data,
    size_t *out_len
) {
    // Strip all control characters from the raw text
    const size_t length_without_control_chars = remove_control_chars(in_data, *in_len);

    // Skip to the start of the exif magic value
    const unsigned char exif_magic[] = {'4', '5', '7', '8', '6', '9', '6', '6', '0', '0', '0', '0'};
    size_t start_index;
    for (start_index = 0; start_index + 6 <= length_without_control_chars; start_index++) if (
        memcmp(in_data + start_index, exif_magic, 6) == 0) break;

    // Make sure the exif magic value was found
    if (start_index + 6 <= length_without_control_chars) {
        // Decode the text from HEX to binary
        size_t decoded_length = 0;
        *out_data = hex_decode(in_data + start_index, length_without_control_chars - start_index, &decoded_length);

        // Find the length of the decoded string terminated by 0xFF 0xD9 if possible
        const uint8_t end_marker[] = {0xFF, 0xD9};
        *out_len = find_terminated_length(*out_data, decoded_length, end_marker, sizeof(end_marker)) + sizeof(
                       end_marker);
    }
}

int load_png(const uint8_t *data, const size_t size, LoadedImage *out_image) {
    // Create a decoder context
    spng_ctx *ctx = spng_ctx_new(0);
    if (!ctx) {
        fprintf(stderr, "Failed to create SPNG context\n");
        return 1;
    }

    if (spng_set_png_buffer(ctx, data, size)) {
        fprintf(stderr, "Failed to set PNG buffer\n");
        spng_ctx_free(ctx);
        return 1;
    }

    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr)) {
        fprintf(stderr, "Failed to get PNG IHDR\n");
        spng_ctx_free(ctx);
        return 1;
    }

    // Allocate output buffer
    size_t image_size;
    if (spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &image_size)) {
        fprintf(stderr, "Failed to get decoded image size\n");
        spng_ctx_free(ctx);
        return 1;
    }

    unsigned char *rgba = malloc(image_size);
    if (!rgba) {
        fprintf(stderr, "Out of memory for decoded image\n");
        spng_ctx_free(ctx);
        return 1;
    }

    if (spng_decode_image(ctx, rgba, image_size, SPNG_FMT_RGBA8, 0)) {
        fprintf(stderr, "Failed to decode PNG image\n");
        free(rgba);
        spng_ctx_free(ctx);
        return 1;
    }

    *out_image = construct_image(
        rgba,
        ihdr.width,
        ihdr.height,
        ihdr.width * 4, // RGBA = 4 bytes per pixel
        true // PNG is lossless
    );

    // Handle metadata — eXIf chunk (PNG spec 1.6.0+)
    static const uint8_t tiff_le[] = {0x49, 0x49, 0x2A, 0x00};
    static const uint8_t tiff_be[] = {0x4D, 0x4D, 0x00, 0x2A};
    struct spng_exif exif_chunk = {0};
    if (spng_get_exif(ctx, &exif_chunk) == 0 && exif_chunk.length > 0) {
        const unsigned char *src = (const unsigned char *)exif_chunk.data;
        for (size_t j = 0; j + 4 <= exif_chunk.length; j++) {
            if (memcmp(src + j, tiff_le, 4) == 0 || memcmp(src + j, tiff_be, 4) == 0) {
                const size_t exif_size = exif_chunk.length - j;
                unsigned char *exif_copy = malloc(exif_size);
                if (exif_copy) {
                    memcpy(exif_copy, src + j, exif_size);
                    out_image->exif_data = exif_copy;
                    out_image->exif_size = exif_size;
                }
                break;
            }
        }
    }

    // Text chunks — XMP (iTXt) and legacy hex-encoded EXIF (tEXt)
    uint32_t n_text = 0;
    const int spng_get_text_err = spng_get_text(ctx, NULL, &n_text);
    if (spng_get_text_err != 0 && spng_get_text_err != SPNG_ECHUNKAVAIL) {
        fprintf(stderr, "Failed to get text chunk count\n");
        spng_ctx_free(ctx);
        return 1;
    }
    if (n_text < 1) goto cleanup;

    struct spng_text *text_chunks = malloc(n_text * sizeof(struct spng_text));
    if (text_chunks == NULL) {
        fprintf(stderr, "Out of memory for text chunks.\n");
        spng_ctx_free(ctx);
        return 1;
    }

    if (spng_get_text(ctx, text_chunks, &n_text) != 0) {
        fprintf(stderr, "Failed to get text chunks\n");
        free(text_chunks);
        return 1;
    }
    for (uint32_t i = 0; i < n_text; i++) {
        const struct spng_text *t = &text_chunks[i];

        if (strcasecmp(t->keyword, XMP_KEYWORD) == 0 && out_image->xmp_data == NULL) {
            unsigned char *xmp = malloc(t->length);
            memcpy(xmp, t->text, t->length);
            out_image->xmp_data = xmp;
            out_image->xmp_size = t->length;
        } else if (strcasecmp(t->keyword, EXIF_KEYWORD) == 0 && out_image->exif_data == NULL) {
            unsigned char *decoded = NULL;
            size_t decoded_len = 0;
            decode_exif_from_hex_string_png(t->text, &t->length, &decoded, &decoded_len);
            if (decoded) {
                for (size_t j = 0; j + 4 <= decoded_len; j++) {
                    if (memcmp(decoded + j, tiff_le, 4) == 0 || memcmp(decoded + j, tiff_be, 4) == 0) {
                        const size_t exif_size = decoded_len - j;
                        unsigned char *exif = malloc(exif_size);
                        if (exif) {
                            memcpy(exif, decoded + j, exif_size);
                            out_image->exif_data = exif;
                            out_image->exif_size = exif_size;
                        }
                        break;
                    }
                }
                free(decoded);
            }
        }
    }
    free(text_chunks);

    cleanup:
    spng_ctx_free(ctx);
    return 0;
}
