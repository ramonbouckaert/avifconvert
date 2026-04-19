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

// Draw one GIF frame onto an RGBA canvas, respecting transparency.
static void composite_frame(uint8_t *canvas, const SavedImage *frame,
                             const ColorMapObject *cmap, int transparent_index,
                             int canvas_w, int canvas_h) {
    const int fx = frame->ImageDesc.Left;
    const int fy = frame->ImageDesc.Top;
    const int fw = frame->ImageDesc.Width;
    const int fh = frame->ImageDesc.Height;

    for (int y = 0; y < fh; y++) {
        const int cy = fy + y;
        if (cy < 0 || cy >= canvas_h) continue;
        for (int x = 0; x < fw; x++) {
            const int cx = fx + x;
            if (cx < 0 || cx >= canvas_w) continue;
            const int idx = (unsigned char)frame->RasterBits[y * fw + x];
            if (idx == transparent_index) continue;
            uint8_t *px = &canvas[(cy * canvas_w + cx) * 4];
            px[0] = cmap->Colors[idx].Red;
            px[1] = cmap->Colors[idx].Green;
            px[2] = cmap->Colors[idx].Blue;
            px[3] = 255;
        }
    }
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

        // Truncate at the end of the XMP document, stripping the magic trailer
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

    const size_t canvas_bytes = (size_t)w * h * 4;

    LoadedFrame *frames = calloc((size_t)gif->ImageCount, sizeof(LoadedFrame));
    if (!frames) {
        DGifCloseFile(gif, &error);
        return 1;
    }

    uint8_t *canvas = calloc(canvas_bytes, 1);
    uint8_t *prev_canvas = NULL;  // saved canvas for DISPOSE_PREVIOUS
    if (!canvas) {
        free(frames);
        DGifCloseFile(gif, &error);
        return 1;
    }

    int prev_disposal = DISPOSAL_UNSPECIFIED;
    int prev_fx = 0, prev_fy = 0, prev_fw = 0, prev_fh = 0;

    for (int fi = 0; fi < gif->ImageCount; fi++) {
        const SavedImage *frame = &gif->SavedImages[fi];

        // Apply the disposal method of the PREVIOUS frame before drawing this one
        if (fi > 0) {
            if (prev_disposal == DISPOSE_BACKGROUND) {
                for (int y = 0; y < prev_fh; y++) {
                    const int cy = prev_fy + y;
                    if (cy < 0 || cy >= h) continue;
                    for (int x = 0; x < prev_fw; x++) {
                        const int cx = prev_fx + x;
                        if (cx < 0 || cx >= w) continue;
                        memset(&canvas[(cy * w + cx) * 4], 0, 4);
                    }
                }
            } else if (prev_disposal == DISPOSE_PREVIOUS && prev_canvas) {
                memcpy(canvas, prev_canvas, canvas_bytes);
            }
            // DISPOSE_DO_NOT / DISPOSAL_UNSPECIFIED: leave canvas as-is
        }

        // Save canvas before drawing this frame if next frame may need DISPOSE_PREVIOUS
        GraphicsControlBlock gcb;
        const int has_gcb = DGifSavedExtensionToGCB(gif, fi, &gcb) == GIF_OK;
        const int this_disposal = has_gcb ? gcb.DisposalMode : DISPOSAL_UNSPECIFIED;
        const int transparent_index = has_gcb ? gcb.TransparentColor : NO_TRANSPARENT_COLOR;
        const unsigned int duration_ms = has_gcb && gcb.DelayTime > 0
                                         ? (unsigned int)gcb.DelayTime * 10
                                         : 100; // default 100ms if unspecified

        if (this_disposal == DISPOSE_PREVIOUS) {
            if (!prev_canvas) {
                prev_canvas = malloc(canvas_bytes);
                if (!prev_canvas) { /* best-effort: fall through without saving */ }
            }
            if (prev_canvas) memcpy(prev_canvas, canvas, canvas_bytes);
        }

        const ColorMapObject *cmap = frame->ImageDesc.ColorMap
                                     ? frame->ImageDesc.ColorMap
                                     : gif->SColorMap;
        if (!cmap) {
            fprintf(stderr, "GIF frame %d has no color map\n", fi);
            // Skip frame — leave canvas unchanged
            goto next_frame;
        }

        composite_frame(canvas, frame, cmap, transparent_index, w, h);

next_frame:
        // Snapshot the current canvas state as this frame's pixel buffer
        frames[fi].pixels = malloc(canvas_bytes);
        if (!frames[fi].pixels) {
            for (int j = 0; j < fi; j++) free(frames[j].pixels);
            free(frames);
            free(canvas);
            free(prev_canvas);
            DGifCloseFile(gif, &error);
            return 1;
        }
        memcpy(frames[fi].pixels, canvas, canvas_bytes);
        frames[fi].duration_ms = duration_ms;

        prev_disposal = this_disposal;
        prev_fx = frame->ImageDesc.Left;
        prev_fy = frame->ImageDesc.Top;
        prev_fw = frame->ImageDesc.Width;
        prev_fh = frame->ImageDesc.Height;
    }

    free(canvas);
    free(prev_canvas);

    *out_image = (LoadedImage){0};
    out_image->width  = (unsigned int)w;
    out_image->height = (unsigned int)h;
    out_image->stride = (unsigned int)w * 4;
    out_image->lossless = false;
    out_image->frames = frames;
    out_image->frame_count = (unsigned int)gif->ImageCount;

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
