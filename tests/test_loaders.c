#include "framework.h"
#include "../src/avif_convert_core/common.h"
#include "../src/avif_convert_core/input/loaders.h"
#include "../src/avif_convert_io/file_io.h"
#include <stdlib.h>

// Not exposed in loaders.h
int detect_webp_lossless(const uint8_t *data, size_t size);

// --- load_png ---

static void test_load_png_succeeds(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_png(data, size, &img), 0);
    ASSERT_NOT_NULL(img.pixels);
    ASSERT_TRUE(img.width > 0);
    ASSERT_TRUE(img.height > 0);
    ASSERT_TRUE(img.lossless);
    free_loaded_image(&img);
    free(data);
}

static void test_load_png_stride_matches_width(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_png(data, size, &img), 0);
    ASSERT_EQ(img.stride, img.width * 4); // RGBA = 4 bytes per pixel
    free_loaded_image(&img);
    free(data);
}

// --- load_jpg ---

static void test_load_jpg_succeeds(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_jpg(data, size, &img), 0);
    ASSERT_NOT_NULL(img.pixels);
    ASSERT_TRUE(img.width > 0);
    ASSERT_TRUE(img.height > 0);
    ASSERT_FALSE(img.lossless); // JPEG is always lossy
    free_loaded_image(&img);
    free(data);
}

static void test_load_jpg_stride_matches_width(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_jpg(data, size, &img), 0);
    ASSERT_EQ(img.stride, img.width * 4); // expanded to RGBA
    free_loaded_image(&img);
    free(data);
}

// --- load_webp ---

static void test_load_webp_succeeds(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_webp(data, size, &img), 0);
    ASSERT_NOT_NULL(img.pixels);
    ASSERT_TRUE(img.width > 0);
    ASSERT_TRUE(img.height > 0);
    ASSERT_FALSE(img.lossless); // burger.webp is VP8 (lossy)
    free_loaded_image(&img);
    free(data);
}

static void test_load_webp_dimensions_match_jpg(void) {
    // The same burger image — PNG, JPG, and WebP should share dimensions
    uint8_t *jpg_data = NULL; size_t jpg_size = 0;
    uint8_t *webp_data = NULL; size_t webp_size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &jpg_data, &jpg_size), 0);
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &webp_data, &webp_size), 0);
    LoadedImage jpg = {0}, webp = {0};
    ASSERT_EQ(load_jpg(jpg_data, jpg_size, &jpg), 0);
    ASSERT_EQ(load_webp(webp_data, webp_size, &webp), 0);
    ASSERT_EQ(jpg.width, webp.width);
    ASSERT_EQ(jpg.height, webp.height);
    free_loaded_image(&jpg); free(jpg_data);
    free_loaded_image(&webp); free(webp_data);
}

// --- load_heif ---

static void test_load_heif_succeeds(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.heif", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_heif(data, size, &img), 0);
    ASSERT_NOT_NULL(img.pixels);
    ASSERT_TRUE(img.width > 0);
    ASSERT_TRUE(img.height > 0);
    ASSERT_FALSE(img.lossless); // HEIC is treated as lossy
    free_loaded_image(&img);
    free(data);
}

// --- metadata extraction ---

static void test_load_jpg_extracts_exif(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_jpg(data, size, &img), 0);
    ASSERT_NOT_NULL(img.exif_data);
    ASSERT_TRUE(img.exif_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_jpg_extracts_xmp(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_jpg(data, size, &img), 0);
    ASSERT_NOT_NULL(img.xmp_data);
    ASSERT_TRUE(img.xmp_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_png_extracts_exif(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_png(data, size, &img), 0);
    ASSERT_NOT_NULL(img.exif_data);
    ASSERT_TRUE(img.exif_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_png_extracts_xmp(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_png(data, size, &img), 0);
    ASSERT_NOT_NULL(img.xmp_data);
    ASSERT_TRUE(img.xmp_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_webp_extracts_exif(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_webp(data, size, &img), 0);
    ASSERT_NOT_NULL(img.exif_data);
    ASSERT_TRUE(img.exif_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_webp_extracts_xmp(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_webp(data, size, &img), 0);
    ASSERT_NOT_NULL(img.xmp_data);
    ASSERT_TRUE(img.xmp_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_heif_extracts_exif(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.heif", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_heif(data, size, &img), 0);
    ASSERT_NOT_NULL(img.exif_data);
    ASSERT_TRUE(img.exif_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_load_heif_extracts_xmp(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.heif", &data, &size), 0);
    LoadedImage img = {0};
    ASSERT_EQ(load_heif(data, size, &img), 0);
    ASSERT_NOT_NULL(img.xmp_data);
    ASSERT_TRUE(img.xmp_size > 0);
    free_loaded_image(&img); free(data);
}

static void test_exif_starts_with_tiff_magic(void) {
    // After normalization, all loaders should return raw TIFF (II or MM magic)
    const uint8_t tiff_le[] = {0x49, 0x49, 0x2A, 0x00}; // "II\x2A\x00"
    const uint8_t tiff_be[] = {0x4D, 0x4D, 0x00, 0x2A}; // "MM\x00\x2A"

    const char *sources[] = {
        TEST_IMAGES_DIR "/burger.jpg",
        TEST_IMAGES_DIR "/burger.png",
        TEST_IMAGES_DIR "/burger.webp",
        TEST_IMAGES_DIR "/burger.heif",
    };
    typedef int (*LoadFn)(const uint8_t *, size_t, LoadedImage *);
    const LoadFn loaders[] = { load_jpg, load_png, load_webp, load_heif };

    for (int i = 0; i < 4; i++) {
        uint8_t *data = NULL; size_t size = 0;
        ASSERT_EQ(read_file(sources[i], &data, &size), 0);
        LoadedImage img = {0};
        ASSERT_EQ(loaders[i](data, size, &img), 0);
        ASSERT_NOT_NULL(img.exif_data);
        ASSERT_TRUE(img.exif_size >= 4);
        const int is_tiff = memcmp(img.exif_data, tiff_le, 4) == 0 ||
                            memcmp(img.exif_data, tiff_be, 4) == 0;
        ASSERT_TRUE(is_tiff);
        free_loaded_image(&img); free(data);
    }
}

// --- detect_webp_lossless ---

static void test_detect_webp_lossless_is_lossy(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &data, &size), 0);
    ASSERT_EQ(detect_webp_lossless(data, size), 0); // VP8 = lossy
    free(data);
}

static void test_detect_webp_lossless_rejects_non_webp(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    ASSERT_EQ(detect_webp_lossless(data, size), -1);
    free(data);
}

static void test_detect_webp_lossless_too_short(void) {
    const uint8_t buf[] = {1, 2, 3, 4};
    ASSERT_EQ(detect_webp_lossless(buf, sizeof(buf)), -1);
}

static void run_all(void) {
    test_load_png_succeeds();
    test_load_png_stride_matches_width();

    test_load_jpg_succeeds();
    test_load_jpg_stride_matches_width();

    test_load_webp_succeeds();
    test_load_webp_dimensions_match_jpg();

    test_load_heif_succeeds();

    test_load_jpg_extracts_exif();
    test_load_jpg_extracts_xmp();
    test_load_png_extracts_exif();
    test_load_png_extracts_xmp();
    test_load_webp_extracts_exif();
    test_load_webp_extracts_xmp();
    test_load_heif_extracts_exif();
    test_load_heif_extracts_xmp();
    test_exif_starts_with_tiff_magic();

    test_detect_webp_lossless_is_lossy();
    test_detect_webp_lossless_rejects_non_webp();
    test_detect_webp_lossless_too_short();
}

int main(void) {
    run_all();
    printf("%d passed, %d failed\n", _tests_passed, _tests_failed);
    return _tests_failed > 0 ? 1 : 0;
}
