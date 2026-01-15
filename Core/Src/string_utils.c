#include "string_utils.h"
#include <string.h>

void safe_copy(char* dest, const char* src, size_t dest_size) {
    if (dest_size == 0) return;

    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

size_t my_strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}
