#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gif_lib.h>

#include "loaders.h"
#include "../common.h"

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} GifMemReader;

static int gif_read_fn(GifFileType *gif, GifByteType *buf, int count) {
    GifMemReader *r = gif->UserData;
    const size_t remaining = r->size - r->pos;
    if ((size_t)count > remaining) count = (int)remaining;
    memcpy(buf, r->data + r->pos, count);
    r->pos += count;
    return count;
}

// GIF stores XMP as an Application Extension: "XMP Data" + "XMP" (11-byte header),
// followed by CONTINUE sub-blocks containing raw XMP XML plus a 258-byte magic trailer.
// We concatenate all CONTINUE blocks and truncate at the closing XMP tag.
static void extract_xmp_from_blocks(const ExtensionBlock *blocks, int count,
                                    unsigned char **xmp_data, size_t *xmp_size) {
    for (int i = 0; i < count - 1; i++) {
        if (blocks[i].Function != APPLICATION_EXT_FUNC_CODE) continue;
        if (blocks[i].ByteCount < 11) continue;
        if (memcmp(blocks[i].Bytes, "XMP Data", 8) != 0) continue;
        if (memcmp(blocks[i].Bytes + 8, "XMP", 3) != 0) continue;

        // Measure total size of following CONTINUE blocks
        size_t total = 0;
        for (int j = i + 1; j < count && blocks[j].Function == CONTINUE_EXT_FUNC_CODE; j++)
            total += (size_t)blocks[j].ByteCount;
        if (total == 0) return;

        unsigned char *buf = malloc(total);
        if (!buf) return;

        size_t pos = 0;
        for (int j = i + 1; j < count && blocks[j].Function == CONTINUE_EXT_FUNC_CODE; j++) {
            memcpy(buf + pos, blocks[j].Bytes, (size_t)blocks[j].ByteCount);
            pos += (size_t)blocks[j].ByteCount;
        }

        // Find the end of the XMP document and truncate the magic trailer
        const char *end_tag = "</x:xmpmeta>";
        const size_t end_tag_len = strlen(end_tag);
        size_t xmp_len = total;
        for (size_t k = total; k >= end_tag_len; k--) {
            if (memcmp(buf + k - end_tag_len, end_tag, end_tag_len) == 0) {
                xmp_len = k;
                break;
            }
        }

        *xmp_data = buf;
        *xmp_size = xmp_len;
        return;
    }
}

int load_gif(const uint8_t *data, const size_t size, LoadedImage *out_image) {
    GifMemReader reader = { data, size, 0 };
    int error;
    GifFileType *gif = DGifOpen(&reader, gif_read_fn, &error);
    if (!gif) {
        fprintf(stderr, "Failed to open GIF: %s\n", GifErrorString(error));
        return 1;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        fprintf(stderr, "Failed to read GIF: %s\n", GifErrorString(gif->Error));
        DGifCloseFile(gif, &error);
        return 1;
    }

    if (gif->ImageCount < 1) {
        fprintf(stderr, "GIF has no frames\n");
        DGifCloseFile(gif, &error);
        return 1;
    }

    const int w = gif->SWidth;
    const int h = gif->SHeight;
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "GIF has invalid dimensions\n");
        DGifCloseFile(gif, &error);
        return 1;
    }

    // Use the first frame only (animated GIF support is out of scope here)
    const SavedImage *frame = &gif->SavedImages[0];
    const ColorMapObject *cmap = frame->ImageDesc.ColorMap
        ? frame->ImageDesc.ColorMap
        : gif->SColorMap;
    if (!cmap) {
        fprintf(stderr, "GIF has no color map\n");
        DGifCloseFile(gif, &error);
        return 1;
    }

    // Check for a transparency index in the Graphic Control Extension
    GraphicsControlBlock gcb;
    const int transparent_index =
        (DGifSavedExtensionToGCB(gif, 0, &gcb) == GIF_OK)
            ? gcb.TransparentColor  // NO_TRANSPARENT_COLOR (-1) if absent
            : NO_TRANSPARENT_COLOR;

    uint8_t *rgba = calloc((size_t)w * h, 4); // initialise canvas to transparent
    if (!rgba) {
        DGifCloseFile(gif, &error);
        return 1;
    }

    // Frame may be positioned within the canvas
    const int fx = frame->ImageDesc.Left;
    const int fy = frame->ImageDesc.Top;
    const int fw = frame->ImageDesc.Width;
    const int fh = frame->ImageDesc.Height;

    for (int y = 0; y < fh; y++) {
        const int cy = fy + y;
        if (cy < 0 || cy >= h) continue;
        for (int x = 0; x < fw; x++) {
            const int cx = fx + x;
            if (cx < 0 || cx >= w) continue;
            const int idx = (unsigned char)frame->RasterBits[y * fw + x];
            if (idx == transparent_index) continue; // leave pixel transparent
            uint8_t *px = &rgba[(cy * w + cx) * 4];
            px[0] = cmap->Colors[idx].Red;
            px[1] = cmap->Colors[idx].Green;
            px[2] = cmap->Colors[idx].Blue;
            px[3] = 255;
        }
    }

    *out_image = construct_image(rgba, (unsigned int)w, (unsigned int)h, (unsigned int)w * 4, false);

    // Extract XMP from extension blocks (check all frames then trailing blocks)
    for (int i = 0; i < gif->ImageCount && !out_image->xmp_data; i++) {
        extract_xmp_from_blocks(gif->SavedImages[i].ExtensionBlocks,
                                gif->SavedImages[i].ExtensionBlockCount,
                                &out_image->xmp_data, &out_image->xmp_size);
    }
    if (!out_image->xmp_data) {
        extract_xmp_from_blocks(gif->ExtensionBlocks, gif->ExtensionBlockCount,
                                &out_image->xmp_data, &out_image->xmp_size);
    }

    DGifCloseFile(gif, &error);
    return 0;
}
