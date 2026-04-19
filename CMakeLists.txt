cmake_minimum_required(VERSION 3.21)
project(avifconvert VERSION 0.2.0 LANGUAGES C)
set(VERSION_SUFFIX "-SNAPSHOT")
set(AVIFCONVERT_VERSION "${PROJECT_VERSION}${VERSION_SUFFIX}")

set(CMAKE_C_STANDARD 99)
set(VCPKG_LIBRARY_LINKAGE static)

find_package(Java)
find_package(JNI)
find_package(libavif CONFIG REQUIRED)
find_package(libheif CONFIG REQUIRED)
find_package(SPNG CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)

if(JNI_FOUND)
    include_directories(${JNI_INCLUDE_DIRS})
endif()

# avifconvertcore

add_library(avifconvertcore SHARED
        src/avif_convert_core/common.h
        src/avif_convert_core/common.c
        src/avif_convert_core/input/loaders.h
        src/avif_convert_core/input/png_loader.c
        src/avif_convert_core/input/heif_loader.c
        src/avif_convert_core/output/avif_writer.c
        src/avif_convert_core/input/jpg_loader.c
        src/avif_convert_core/input/webp_loader.c
        src/avif_convert_core/avif_convert.c
        src/avif_convert_core/avif_convert.h
)

# Uncomment the following line when building avifconvertcore as a SHARED library rather than STATIC
target_compile_definitions(avifconvertcore PRIVATE AVIFCONVERTCORE_SHARED AVIFCONVERTCORE_EXPORTS)

target_link_libraries(avifconvertcore PRIVATE
        avif
        heif
        JPEG::JPEG
        WebP::webp WebP::webpdecoder WebP::webpdemux
        $<IF:$<TARGET_EXISTS:spng::spng>,spng::spng,spng::spng_static>
)

# avifconvert-jni (optional — requires Java)

if(Java_FOUND AND JNI_FOUND)
    add_library(avifconvert-jni SHARED
            src/avif_convert_jni/avif_convert_jni.h
            src/avif_convert_jni/avif_convert_jni.c
    )
    target_link_libraries(avifconvert-jni PUBLIC avifconvertcore)
endif()

# avifconvert-io

add_library(avifconvertio SHARED
        src/avif_convert_io/file_io.h
        src/avif_convert_io/file_io.c
)

# Uncomment the following line when building avifconvertio as a SHARED library rather than STATIC
target_compile_definitions(avifconvertio PRIVATE AVIFCONVERTIO_SHARED AVIFCONVERTIO_EXPORTS)

# avifconvert executable

add_executable(avifconvert src/avif_convert/main.c)

target_compile_definitions(avifconvert PRIVATE AVIFCONVERT_VERSION="${AVIFCONVERT_VERSION}")

target_link_libraries(avifconvert PUBLIC avifconvertcore avifconvertio)

# avifconvertwatch executable

add_executable(avifconvertwatch src/avif_convert_watch/main.c
        src/avif_convert_watch/file_watcher.c
        src/avif_convert_watch/file_watcher.h
)

target_compile_definitions(avifconvertwatch PRIVATE AVIFCONVERT_VERSION="${AVIFCONVERT_VERSION}")

target_link_libraries(avifconvertwatch PUBLIC avifconvertcore avifconvertio
        PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
)

# Hacks

if(UNIX AND NOT APPLE)
    target_link_libraries(avifconvertcore PRIVATE stdc++)
endif()

# Tests

enable_testing()

foreach(suite common file_io loaders convert)
    add_executable(test-${suite} tests/test_${suite}.c)
    target_compile_definitions(test-${suite} PRIVATE
            TEST_IMAGES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/images"
    )
    target_link_libraries(test-${suite} PRIVATE avifconvertcore avifconvertio avif)
    add_test(NAME ${suite} COMMAND test-${suite})
endforeach()
