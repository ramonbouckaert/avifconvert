#include "framework.h"
#include "../src/avif_convert_core/avif_convert.h"
#include "../src/avif_convert_io/file_io.h"
#include <avif/avif.h>
#include <stdlib.h>
#include <string.h>

static int bytes_contain(const uint8_t *data, size_t size, const char *needle) {
    const size_t nlen = strlen(needle);
    if (nlen > size) return 0;
    for (size_t i = 0; i <= size - nlen; i++) {
        if (memcmp(data + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

static void assert_is_valid_exif(const uint8_t *data, size_t size) {
    static const uint8_t tiff_le[] = {0x49, 0x49, 0x2A, 0x00};
    static const uint8_t tiff_be[] = {0x4D, 0x4D, 0x00, 0x2A};
    ASSERT_TRUE(size >= 8);
    ASSERT_TRUE(memcmp(data, tiff_le, 4) == 0 || memcmp(data, tiff_be, 4) == 0);
    ASSERT_TRUE(bytes_contain(data, size, "NIKON CORPORATION"));
}

static void assert_is_valid_xmp(const uint8_t *data, size_t size) {
    ASSERT_TRUE(size > 0);
    ASSERT_TRUE(bytes_contain(data, size, "xmpmeta"));
    ASSERT_TRUE(bytes_contain(data, size, "cheeseburger"));
}

static void assert_is_valid_avif(const uint8_t *data, size_t size) {
    ASSERT_TRUE(size >= 12);
    ASSERT_MEM_EQ(data + 4, "ftyp", 4);
    ASSERT_TRUE(
        memcmp(data + 8, "avif", 4) == 0 ||
        memcmp(data + 8, "avis", 4) == 0
    );

    // Check dimensions match the reference burger.avif (loaded once).
    static uint32_t ref_w = 0, ref_h = 0;
    if (ref_w == 0) {
        uint8_t *ref_data = NULL; size_t ref_size = 0;
        if (read_file(TEST_IMAGES_DIR "/burger.avif", &ref_data, &ref_size) == 0) {
            avifDecoder *ref = avifDecoderCreate();
            if (avifDecoderSetIOMemory(ref, ref_data, ref_size) == AVIF_RESULT_OK &&
                avifDecoderParse(ref) == AVIF_RESULT_OK) {
                ref_w = ref->image->width;
                ref_h = ref->image->height;
            }
            avifDecoderDestroy(ref);
            free(ref_data);
        }
    }
    if (ref_w > 0) {
        avifDecoder *dec = avifDecoderCreate();
        if (avifDecoderSetIOMemory(dec, data, size) == AVIF_RESULT_OK &&
            avifDecoderParse(dec) == AVIF_RESULT_OK) {
            ASSERT_EQ(dec->image->width, ref_w);
            ASSERT_EQ(dec->image->height, ref_h);
        }
        avifDecoderDestroy(dec);
    }
}

// Decode an AVIF blob and return the decoder (caller must avifDecoderDestroy).
// Returns NULL and records a test failure if decoding fails.
static avifDecoder *decode_avif(const uint8_t *data, size_t size) {
    avifDecoder *decoder = avifDecoderCreate();
    avifResult result = avifDecoderSetIOMemory(decoder, data, size);
    if (result == AVIF_RESULT_OK) result = avifDecoderParse(decoder);
    if (result == AVIF_RESULT_OK) result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK) {
        fprintf(stderr, "  avif decode failed: %s\n", avifResultToString(result));
        avifDecoderDestroy(decoder);
        _tests_failed++;
        return NULL;
    }
    return decoder;
}

// --- format tests ---

static void test_convert_jpg(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    assert_is_valid_avif(out, out_size);
    free(in); free(out);
}

static void test_convert_png(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    assert_is_valid_avif(out, out_size);
    free(in); free(out);
}

static void test_convert_webp(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    assert_is_valid_avif(out, out_size);
    free(in); free(out);
}

static void test_convert_heif(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.heif", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    assert_is_valid_avif(out, out_size);
    free(in); free(out);
}

static void test_convert_animated_gif(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger_anim.gif", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    // Animated AVIF uses the 'avis' brand
    ASSERT_TRUE(out_size >= 12);
    ASSERT_MEM_EQ(out + 4, "ftyp", 4);
    ASSERT_TRUE(memcmp(out + 8, "avis", 4) == 0 || memcmp(out + 8, "avif", 4) == 0);

    // Decode and verify multiple frames
    avifDecoder *dec = avifDecoderCreate();
    ASSERT_EQ(avifDecoderSetIOMemory(dec, out, out_size), AVIF_RESULT_OK);
    ASSERT_EQ(avifDecoderParse(dec), AVIF_RESULT_OK);
    ASSERT_TRUE(dec->imageCount >= 2);
    avifDecoderDestroy(dec);

    free(in); free(out);
}

static void test_convert_avif_passthrough(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.avif", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    ASSERT_EQ(out_size, in_size);
    ASSERT_MEM_EQ(out, in, in_size);
    free(in); free(out);
}

static void test_convert_gif(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.gif", &in, &in_size), 0);
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &out, &out_size, 10, 80), 0);
    assert_is_valid_avif(out, out_size);
    free(in); free(out);
}

static void test_convert_unknown_format_fails(void) {
    const uint8_t garbage[] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C};
    uint8_t *out = NULL; size_t out_size = 0;
    ASSERT_NE(convert_to_avif(garbage, sizeof(garbage), &out, &out_size, 10, 80), 0);
}

static void test_convert_quality_affects_output_size(void) {
    uint8_t *in = NULL; size_t in_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &in, &in_size), 0);
    uint8_t *lo = NULL; size_t lo_size = 0;
    uint8_t *hi = NULL; size_t hi_size = 0;
    ASSERT_EQ(convert_to_avif(in, in_size, &lo, &lo_size, 10, 40), 0);
    ASSERT_EQ(convert_to_avif(in, in_size, &hi, &hi_size, 10, 90), 0);
    ASSERT_TRUE(lo_size < hi_size);
    free(in); free(lo); free(hi);
}

// --- metadata tests ---

// Convert a source file and return the decoded AVIF (caller must avifDecoderDestroy + free out).
// Returns NULL on failure.
static avifDecoder *convert_and_decode(const char *src_path, uint8_t **out, size_t *out_size) {
    uint8_t *in = NULL; size_t in_size = 0;
    if (read_file(src_path, &in, &in_size) != 0) { _tests_failed++; return NULL; }
    if (convert_to_avif(in, in_size, out, out_size, 10, 80) != 0) { free(in); _tests_failed++; return NULL; }
    free(in);
    return decode_avif(*out, *out_size);
}

static void test_jpg_preserves_exif(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.jpg", &out, &out_size);
    if (!dec) return;
    assert_is_valid_exif(dec->image->exif.data, dec->image->exif.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_jpg_preserves_xmp(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.jpg", &out, &out_size);
    if (!dec) return;
    assert_is_valid_xmp(dec->image->xmp.data, dec->image->xmp.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_png_preserves_exif(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.png", &out, &out_size);
    if (!dec) return;
    assert_is_valid_exif(dec->image->exif.data, dec->image->exif.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_png_preserves_xmp(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.png", &out, &out_size);
    if (!dec) return;
    assert_is_valid_xmp(dec->image->xmp.data, dec->image->xmp.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_webp_preserves_exif(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.webp", &out, &out_size);
    if (!dec) return;
    assert_is_valid_exif(dec->image->exif.data, dec->image->exif.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_webp_preserves_xmp(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.webp", &out, &out_size);
    if (!dec) return;
    assert_is_valid_xmp(dec->image->xmp.data, dec->image->xmp.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_heif_preserves_exif(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.heif", &out, &out_size);
    if (!dec) return;
    assert_is_valid_exif(dec->image->exif.data, dec->image->exif.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_gif_preserves_xmp(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.gif", &out, &out_size);
    if (!dec) return;
    assert_is_valid_xmp(dec->image->xmp.data, dec->image->xmp.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_heif_preserves_xmp(void) {
    uint8_t *out = NULL; size_t out_size = 0;
    avifDecoder *dec = convert_and_decode(TEST_IMAGES_DIR "/burger.heif", &out, &out_size);
    if (!dec) return;
    assert_is_valid_xmp(dec->image->xmp.data, dec->image->xmp.size);
    avifDecoderDestroy(dec); free(out);
}

static void test_exif_preserved_across_formats(void) {
    const char *sources[] = {
        TEST_IMAGES_DIR "/burger.jpg",
        TEST_IMAGES_DIR "/burger.png",
        TEST_IMAGES_DIR "/burger.webp",
        TEST_IMAGES_DIR "/burger.heif",
    };
    const int n = 4;

    uint8_t *out[4] = {0}; size_t out_size[4] = {0};
    avifDecoder *dec[4] = {0};

    for (int i = 0; i < n; i++) {
        dec[i] = convert_and_decode(sources[i], &out[i], &out_size[i]);
        if (!dec[i]) goto cleanup;
    }

    for (int i = 0; i < n; i++) {
        assert_is_valid_exif(dec[i]->image->exif.data, dec[i]->image->exif.size);
    }

cleanup:
    for (int i = 0; i < n; i++) {
        if (dec[i]) avifDecoderDestroy(dec[i]);
        free(out[i]);
    }
}

static void test_xmp_preserved_across_formats(void) {
    const char *sources[] = {
        TEST_IMAGES_DIR "/burger.jpg",
        TEST_IMAGES_DIR "/burger.png",
        TEST_IMAGES_DIR "/burger.webp",
        TEST_IMAGES_DIR "/burger.heif",
    };
    const int n = 4;

    uint8_t *out[4] = {0}; size_t out_size[4] = {0};
    avifDecoder *dec[4] = {0};

    for (int i = 0; i < n; i++) {
        dec[i] = convert_and_decode(sources[i], &out[i], &out_size[i]);
        if (!dec[i]) goto cleanup;
    }

    for (int i = 0; i < n; i++) {
        assert_is_valid_xmp(dec[i]->image->xmp.data, dec[i]->image->xmp.size);
    }

cleanup:
    for (int i = 0; i < n; i++) {
        if (dec[i]) avifDecoderDestroy(dec[i]);
        free(out[i]);
    }
}

static void run_all(void) {
    test_convert_jpg();
    test_convert_png();
    test_convert_webp();
    test_convert_heif();
    test_convert_gif();
    test_convert_animated_gif();
    test_convert_avif_passthrough();
    test_convert_unknown_format_fails();
    test_convert_quality_affects_output_size();

    test_jpg_preserves_exif();
    test_jpg_preserves_xmp();
    test_png_preserves_exif();
    test_png_preserves_xmp();
    test_webp_preserves_exif();
    test_webp_preserves_xmp();
    test_heif_preserves_exif();
    test_heif_preserves_xmp();
    test_gif_preserves_xmp();
    test_exif_preserved_across_formats();
    test_xmp_preserved_across_formats();
}

int main(void) {
    run_all();
    printf("%d passed, %d failed\n", _tests_passed, _tests_failed);
    return _tests_failed > 0 ? 1 : 0;
}
