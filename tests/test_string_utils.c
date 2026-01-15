#include "unity.h"
#include <string.h>

// Include source directly (no HAL dependencies)
#include "../Core/Src/string_utils.c"

void setUp(void) {}
void tearDown(void) {}

void test_strlen_empty_string(void) {
    TEST_ASSERT_EQUAL(0, my_strlen(""));
}

void test_strlen_normal_string(void) {
    TEST_ASSERT_EQUAL(5, my_strlen("hello"));
}

void test_safe_copy_normal(void) {
    char dest[10];
    safe_copy(dest, "hello", sizeof(dest));
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

void test_safe_copy_truncate(void) {
    char dest[5];
    safe_copy(dest, "hello world", sizeof(dest));
    TEST_ASSERT_EQUAL_STRING("hell", dest);
    TEST_ASSERT_EQUAL(4, strlen(dest));
}

void test_safe_copy_exact_fit(void) {
    char dest[6];
    safe_copy(dest, "hello", sizeof(dest));
    TEST_ASSERT_EQUAL_STRING("hello", dest);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_strlen_empty_string);
    RUN_TEST(test_strlen_normal_string);
    RUN_TEST(test_safe_copy_normal);
    RUN_TEST(test_safe_copy_truncate);
    RUN_TEST(test_safe_copy_exact_fit);
    return UNITY_END();
}
