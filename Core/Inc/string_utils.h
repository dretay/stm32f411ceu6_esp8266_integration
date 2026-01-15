#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

// Safe string copy (demonstrating buffer safety)
void safe_copy(char* dest, const char* src, size_t dest_size);

// String length (pure function, easy to test)
size_t my_strlen(const char* str);

#endif // STRING_UTILS_H
