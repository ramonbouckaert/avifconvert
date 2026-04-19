#include "framework.h"
#include "../src/avif_convert_io/file_io.h"
#include <stdlib.h>
#include <string.h>

static void test_make_avif_path_jpg(void) {
    char *out = make_avif_path("photo.jpg");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "photo.avif");
    free(out);
}

static void test_make_avif_path_png(void) {
    char *out = make_avif_path("image.png");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "image.avif");
    free(out);
}

static void test_make_avif_path_webp(void) {
    char *out = make_avif_path("photo.webp");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "photo.avif");
    free(out);
}

static void test_make_avif_path_heic(void) {
    char *out = make_avif_path("photo.heic");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "photo.avif");
    free(out);
}

static void test_make_avif_path_uppercase_ext(void) {
    char *out = make_avif_path("photo.JPG");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "photo.AVIF");
    free(out);
}

static void test_make_avif_path_uppercase_png(void) {
    char *out = make_avif_path("image.PNG");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "image.AVIF");
    free(out);
}

static void test_make_avif_path_no_extension(void) {
    char *out = make_avif_path("imagefile");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "imagefile.avif");
    free(out);
}

static void test_make_avif_path_with_directory(void) {
    char *out = make_avif_path("/path/to/photo.webp");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "/path/to/photo.avif");
    free(out);
}

static void test_make_avif_path_null_returns_null(void) {
    ASSERT_NULL(make_avif_path(NULL));
}

static void test_make_avif_path_already_avif(void) {
    char *out = make_avif_path("image.avif");
    ASSERT_NOT_NULL(out);
    ASSERT_STR_EQ(out, "image.avif");
    free(out);
}

static void run_all(void) {
    test_make_avif_path_jpg();
    test_make_avif_path_png();
    test_make_avif_path_webp();
    test_make_avif_path_heic();
    test_make_avif_path_uppercase_ext();
    test_make_avif_path_uppercase_png();
    test_make_avif_path_no_extension();
    test_make_avif_path_with_directory();
    test_make_avif_path_null_returns_null();
    test_make_avif_path_already_avif();
}

int main(void) {
    run_all();
    printf("%d passed, %d failed\n", _tests_passed, _tests_failed);
    return _tests_failed > 0 ? 1 : 0;
}
