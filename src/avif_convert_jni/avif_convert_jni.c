//
// Created by Ramon on 02/06/2025.
//

#include <jni.h>
#include <stdint.h>
#include <stdlib.h>

#include "../avif_convert_core/avif_convert.h"

JNIEXPORT jbyteArray JNICALL Java_com_io_bouckaert_avif_convert_convertToAvif(
    JNIEnv *env,
    const jclass clazz,
    const jbyteArray input
) {
    // Get input data and length
    jsize input_size = (*env)->GetArrayLength(env, input);
    jbyte* input_bytes = (*env)->GetByteArrayElements(env, input, NULL);
    if (!input_bytes) return NULL;

    // Prepare output pointers
    uint8_t* output_data = NULL;
    size_t output_size = 0;

    // Call C function
    const int result = convert_to_avif((const uint8_t*)input_bytes, input_size, &output_data, &output_size, 5, 80);

    (*env)->ReleaseByteArrayElements(env, input, input_bytes, JNI_ABORT);

    if (result != 0 || output_data == NULL || output_size == 0) {
        return NULL;
    }

    // Create Java byte array and copy result
    const jbyteArray output_array = (*env)->NewByteArray(env, (jsize) output_size);
    (*env)->SetByteArrayRegion(env, output_array, 0, (jsize) output_size, (jbyte*) output_data);

    // Clean up C-allocated memory
    free(output_data);

    return output_array;
}
