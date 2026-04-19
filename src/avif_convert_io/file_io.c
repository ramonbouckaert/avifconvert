//
// Created by Ramon on 02/06/2025.
//
#include "file_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if defined(_WIN32)
#include <windows.h>
#endif

#define AVIF_EXTENSION ".avif"

AVIF_CONVERT_IO_API char *make_avif_path(const char *input_path) {
    if (!input_path) return NULL;

    const char *dot = strrchr(input_path, '.');
    const size_t base_len = dot ? (size_t)(dot - input_path) : strlen(input_path);
    const char *ext = AVIF_EXTENSION;
    const size_t ext_len = strlen(ext);
    const size_t total_len = base_len + ext_len + 1;  // +1 for null terminator

    // Determine desired case for the new extension
    int use_uppercase = 0;
    if (dot && isalpha(dot[1]) && isupper((unsigned char)dot[1])) {
        use_uppercase = 1;
    }

    char *output_path = malloc(total_len);
    if (!output_path) return NULL;

    // Copy base path
    memcpy(output_path, input_path, base_len);
    // Append extension
    for (size_t i = 0; i < ext_len; ++i) {
        output_path[base_len + i] = use_uppercase ? (char) toupper((unsigned char) ext[i]) // NOLINT(*-narrowing-conversions)
                                                  : (char) tolower((unsigned char) ext[i]); // NOLINT(*-narrowing-conversions)
    }
    output_path[base_len + ext_len] = '\0'; // Finish with null terminator

    return output_path;
}

AVIF_CONVERT_IO_API int read_file(const char *path, uint8_t **out_data, size_t *out_size) {
    FILE *f;
    portable_fopen(&f, path, "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);

    if (size < 0) {
        fclose(f);
        return 1;
    }

    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return 1;
    }

    if (fread(data, 1, size, f) != (size_t) size) {
        fclose(f);
        return 1;
    }

    fclose(f);
    *out_data = data;
    *out_size = (size_t) size;
    return 0;
}

AVIF_CONVERT_IO_API int write_file(const char *path, uint8_t const *data, const size_t size) {
    FILE *f = NULL;
    const int err = portable_fopen(&f, path, "wb");
    if (f == NULL || err != 0) {
        fprintf(stderr, "Failed to open output file: %s\n", strerror(err));
        return 1;
    }
    if (fwrite(data, 1, size, f) != size) {
        fprintf(stderr, "Failed to write output file: %s\n", path);
        fclose(f);
        return 1;
    }
    fclose(f);
    printf("Wrote %s (%zu bytes)\n", path, size);
    return 0;
}

#if defined(_WIN32)
AVIF_CONVERT_IO_API int copy_file_times(const char *src, const char *dst) {
    const HANDLE hSrc = CreateFileA(src, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) return -1;

    FILETIME creationTime;
    FILETIME lastAccessTime;
    FILETIME lastWriteTime;
    if (!GetFileTime(hSrc, &creationTime, &lastAccessTime, &lastWriteTime)) {
        CloseHandle(hSrc);
        return -1;
    }
    CloseHandle(hSrc);

    HANDLE hDst = CreateFileA(dst, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hDst == INVALID_HANDLE_VALUE) return -1;

    const BOOL result = SetFileTime(hDst, &creationTime, &lastAccessTime, &lastWriteTime);
    CloseHandle(hDst);
    return result ? 0 : -1;
}

#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

AVIF_CONVERT_IO_API int copy_file_times(const char *src, const char *dst) {
    struct stat src_stat;
    if (stat(src, &src_stat) != 0) return -1;

#if defined(__APPLE__)
    struct timespec times[2];
    times[0] = src_stat.st_atimespec;
    times[1] = src_stat.st_mtimespec;
#else
    struct timespec times[2];
    times[0] = src_stat.st_atim;
    times[1] = src_stat.st_mtim;
#endif

    if (utimensat(AT_FDCWD, dst, times, 0) != 0) return -1;

    return 0;
}
#endif