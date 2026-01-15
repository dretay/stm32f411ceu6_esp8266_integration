#include "unity.h"
#include "hal_compat.h"  // Must be before DateHelper.h
#include "DateHelper.h"
#include <string.h>

// Unity setUp and tearDown
void setUp(void) {
    // Set up test fixtures here if needed
}

void tearDown(void) {
    // Clean up after each test
}

// Test cases
void test_to_string_format(void) {
    char buffer[10];
    DateHelper.to_string(buffer);

    // The buffer should contain a formatted time string
    // Format is "H:MM AM/PM" so minimum length is 7 characters
    TEST_ASSERT_TRUE(strlen(buffer) >= 7);

    // Should contain either "AM" or "PM"
    TEST_ASSERT_TRUE(strstr(buffer, "AM") != NULL || strstr(buffer, "PM") != NULL);

    // Should contain ":"
    TEST_ASSERT_TRUE(strchr(buffer, ':') != NULL);
}

void test_get_year(void) {
    int year = DateHelper.get_year();

    // Year should be in a reasonable range (assuming mock returns 2024)
    TEST_ASSERT_TRUE(year >= 2000 && year <= 2100);
}

void test_get_epoch(void) {
    time_t epoch = DateHelper.get_epoch();

    // Epoch time should be positive (after Jan 1, 1970)
    TEST_ASSERT_TRUE(epoch > 0);
}

void test_buffer_overflow_protection(void) {
    char small_buffer[10];

    // This should not crash (snprintf protects against overflow)
    DateHelper.to_string(small_buffer);

    // Buffer should be null-terminated
    TEST_ASSERT_TRUE(small_buffer[9] == '\0' || strlen(small_buffer) < 10);
}

// Main test runner
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_to_string_format);
    RUN_TEST(test_get_year);
    RUN_TEST(test_get_epoch);
    RUN_TEST(test_buffer_overflow_protection);

    return UNITY_END();
}
