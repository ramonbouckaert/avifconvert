#include "framework.h"
#include "../src/avif_convert_core/common.h"
#include "../src/avif_convert_io/file_io.h"
#include <stdlib.h>
#include <string.h>

// --- detect_input_format (real files) ---

static void test_detect_png_file(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.png", &data, &size), 0);
    ASSERT_EQ(detect_input_format(data, size), FORMAT_PNG);
    free(data);
}

static void test_detect_jpeg_file(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.jpg", &data, &size), 0);
    ASSERT_EQ(detect_input_format(data, size), FORMAT_JPEG);
    free(data);
}

static void test_detect_webp_file(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.webp", &data, &size), 0);
    ASSERT_EQ(detect_input_format(data, size), FORMAT_WEBP);
    free(data);
}

static void test_detect_heif_file(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.heif", &data, &size), 0);
    ASSERT_EQ(detect_input_format(data, size), FORMAT_HEIF);
    free(data);
}

static void test_detect_avif_file(void) {
    uint8_t *data = NULL; size_t size = 0;
    ASSERT_EQ(read_file(TEST_IMAGES_DIR "/burger.avif", &data, &size), 0);
    ASSERT_EQ(detect_input_format(data, size), FORMAT_AVIF);
    free(data);
}

// Synthetic edge cases — no real sample file for these
static void test_detect_unknown_bytes(void) {
    const uint8_t buf[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0,0,0,0};
    ASSERT_EQ(detect_input_format(buf, sizeof(buf)), FORMAT_UNKNOWN);
}

static void test_detect_too_short(void) {
    const uint8_t buf[] = {0xFF, 0xD8};
    ASSERT_EQ(detect_input_format(buf, sizeof(buf)), FORMAT_UNKNOWN);
}

static void test_detect_heif_all_brands(void) {
    const char *brands[] = {"heix", "hevc", "hevx", "mif1", "msf1"};
    for (int i = 0; i < 5; i++) {
        uint8_t buf[12] = {0,0,0,0, 'f','t','y','p', 0,0,0,0};
        memcpy(buf + 8, brands[i], 4);
        ASSERT_EQ(detect_input_format(buf, 12), FORMAT_HEIF);
    }
}

static void test_detect_avis_brand(void) {
    const uint8_t buf[] = {0,0,0,0, 'f','t','y','p', 'a','v','i','s'};
    ASSERT_EQ(detect_input_format(buf, sizeof(buf)), FORMAT_AVIF);
}

// --- remove_control_chars ---

static void test_remove_control_chars_strips_control(void) {
    char buf[8] = {'a', 'b', 0x01, 0x1f, 0x7f, 'c', 'd', (char)0x9f};
    const size_t new_len = remove_control_chars(buf, 8);
    ASSERT_EQ(new_len, 4u);
    ASSERT_TRUE(buf[0] == 'a' && buf[1] == 'b' && buf[2] == 'c' && buf[3] == 'd');
}

static void test_remove_control_chars_clean_string_unchanged(void) {
    char buf[] = "hello";
    ASSERT_EQ(remove_control_chars(buf, 5), 5u);
    ASSERT_TRUE(memcmp(buf, "hello", 5) == 0);
}

static void test_remove_control_chars_empty(void) {
    char buf[] = "";
    ASSERT_EQ(remove_control_chars(buf, 0), 0u);
}

static void test_remove_control_chars_all_control(void) {
    char buf[3] = {0x01, 0x0a, 0x1f};
    ASSERT_EQ(remove_control_chars(buf, 3), 0u);
}

// --- hex_decode ---

static void test_hex_decode_valid_lowercase(void) {
    size_t out_len = 0;
    unsigned char *out = hex_decode("48656c6c6f", 10, &out_len);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out_len, 5u);
    ASSERT_MEM_EQ(out, "Hello", 5);
    free(out);
}

static void test_hex_decode_valid_uppercase(void) {
    size_t out_len = 0;
    unsigned char *out = hex_decode("DEADBEEF", 8, &out_len);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out_len, 4u);
    ASSERT_EQ(out[0], 0xDE); ASSERT_EQ(out[1], 0xAD);
    ASSERT_EQ(out[2], 0xBE); ASSERT_EQ(out[3], 0xEF);
    free(out);
}

static void test_hex_decode_odd_length_returns_null(void) {
    size_t out_len = 0;
    ASSERT_NULL(hex_decode("abc", 3, &out_len));
}

static void test_hex_decode_invalid_char_returns_null(void) {
    size_t out_len = 0;
    ASSERT_NULL(hex_decode("GG", 2, &out_len));
}

static void test_hex_decode_empty(void) {
    size_t out_len = 99;
    unsigned char *out = hex_decode("", 0, &out_len);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out_len, 0u);
    free(out);
}

static void test_hex_decode_null_out_len(void) {
    unsigned char *out = hex_decode("ff", 2, NULL);
    ASSERT_NOT_NULL(out);
    ASSERT_EQ(out[0], 0xFF);
    free(out);
}

// --- find_terminated_length ---

static void test_find_terminated_length_found_middle(void) {
    const uint8_t data[]   = {0xAA, 0xBB, 0xFF, 0xD9, 0xCC};
    const uint8_t marker[] = {0xFF, 0xD9};
    ASSERT_EQ(find_terminated_length(data, 5, marker, 2), 2u);
}

static void test_find_terminated_length_found_at_start(void) {
    const uint8_t data[]   = {0xFF, 0xD9, 0xAA};
    const uint8_t marker[] = {0xFF, 0xD9};
    ASSERT_EQ(find_terminated_length(data, 3, marker, 2), 0u);
}

static void test_find_terminated_length_not_found(void) {
    const uint8_t data[]   = {0xAA, 0xBB, 0xCC};
    const uint8_t marker[] = {0xFF, 0xD9};
    ASSERT_EQ(find_terminated_length(data, 3, marker, 2), 3u);
}

static void test_find_terminated_length_empty_needle(void) {
    const uint8_t data[] = {0xAA, 0xBB};
    ASSERT_EQ(find_terminated_length(data, 2, data, 0), 2u);
}

static void test_find_terminated_length_needle_longer_than_haystack(void) {
    const uint8_t data[]   = {0xAA};
    const uint8_t marker[] = {0xAA, 0xBB};
    ASSERT_EQ(find_terminated_length(data, 1, marker, 2), 1u);
}

static void run_all(void) {
    test_detect_png_file();
    test_detect_jpeg_file();
    test_detect_webp_file();
    test_detect_heif_file();
    test_detect_avif_file();
    test_detect_unknown_bytes();
    test_detect_too_short();
    test_detect_heif_all_brands();
    test_detect_avis_brand();

    test_remove_control_chars_strips_control();
    test_remove_control_chars_clean_string_unchanged();
    test_remove_control_chars_empty();
    test_remove_control_chars_all_control();

    test_hex_decode_valid_lowercase();
    test_hex_decode_valid_uppercase();
    test_hex_decode_odd_length_returns_null();
    test_hex_decode_invalid_char_returns_null();
    test_hex_decode_empty();
    test_hex_decode_null_out_len();

    test_find_terminated_length_found_middle();
    test_find_terminated_length_found_at_start();
    test_find_terminated_length_not_found();
    test_find_terminated_length_empty_needle();
    test_find_terminated_length_needle_longer_than_haystack();
}

int main(void) {
    run_all();
    printf("%d passed, %d failed\n", _tests_passed, _tests_failed);
    return _tests_failed > 0 ? 1 : 0;
}
