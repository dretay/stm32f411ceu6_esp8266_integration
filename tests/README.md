# Unit Testing and Static Analysis

This directory contains the unit testing infrastructure for the STM32 project.

## ⚠️ Current Status

The testing framework is **set up** but requires code refactoring to make modules testable. The challenge with embedded code is that many modules tightly couple to HAL headers, which can't be compiled natively on macOS.

## Setup Complete

✅ **Unity** test framework (v2.6.0) installed  
✅ **Cppcheck** static analysis tool installed  
✅ **AddressSanitizer** and **UndefinedBehaviorSanitizer** configured  
✅ VS Code tasks for testing and analysis  

## Quick Start - Static Analysis

Cppcheck works immediately without code changes:

```bash
# Via VS Code
Cmd+Shift+P → Tasks: Run Task → "Run Cppcheck"

# Via command line
cppcheck --enable=all --std=c11 \
  --suppressions-list=.cppcheck-suppressions \
  -ICore/Inc -IDrivers/STM32F4xx_HAL_Driver/Inc \
  Core/Src
```

## Writing Testable Code

To make code testable on the host (macOS), follow these patterns:

### ❌ Hard to Test (Tight Coupling)
```c
// DateHelper.c - directly uses HAL
#include "stm32f4xx_hal.h"

void my_function() {
    RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    // ... use time ...
}
```

### ✅ Easy to Test (Dependency Injection)
```c
// calculator.h
int add(int a, int b);
int multiply(int a, int b);

// calculator.c
int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }

// test_calculator.c
#include "unity.h"
#include "calculator.h"

void test_add() {
    TEST_ASSERT_EQUAL(5, add(2, 3));
}

void test_multiply() {
    TEST_ASSERT_EQUAL(6, multiply(2, 3));
}
```

## Test Example Structure

Create `tests/test_calculator.c`:

```c
#include "unity.h"
#include "calculator.h"  // Pure C logic, no HAL dependencies

void setUp(void) {}
void tearDown(void) {}

void test_addition_positive_numbers(void) {
    TEST_ASSERT_EQUAL(5, add(2, 3));
}

void test_addition_negative_numbers(void) {
    TEST_ASSERT_EQUAL(-5, add(-2, -3));
}

void test_multiply_by_zero(void) {
    TEST_ASSERT_EQUAL(0, multiply(5, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_addition_positive_numbers);
    RUN_TEST(test_addition_negative_numbers);
    RUN_TEST(test_multiply_by_zero);
    return UNITY_END();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_calculator
    test_calculator.c
    ../Core/Src/calculator.c  # Your source file
)
target_include_directories(test_calculator PRIVATE
    ../Core/Inc
)
target_link_libraries(test_calculator unity)
add_test(NAME Calculator COMMAND test_calculator)
```

## Running Tests

```bash
# Build tests
cmake -S tests -B tests/build
cmake --build tests/build

# Run all tests
ctest --test-dir tests/build --output-on-failure

# Run with sanitizers (detects memory leaks, buffer overflows)
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir tests/build --output-on-failure
```

## VS Code Tasks

- **Configure Tests** - Set up CMake
- **Build Tests** - Compile test executables
- **Run Tests** - Execute all tests
- **Run Tests with Sanitizers** - Run with memory error detection
- **Run Cppcheck** - Static code analysis

## What Can Be Tested?

### ✅ Easily Testable
- Pure algorithms (sorting, searching, calculations)
- Data structures (linked lists, buffers, parsers)
- String manipulation
- Protocol encoders/decoders
- State machines
- Business logic

### ⚠️ Requires Mocking
- Functions using HAL (GPIO, RTC, SPI, etc.)
- Interrupt handlers
- DMA operations

### 🚫 Better Tested On-Target
- Timing-critical code
- Hardware initialization
- Low-level register access
- Real-time constraints

## Refactoring for Testability

Split hardware-dependent and hardware-independent code:

```c
// time_formatter.c - Pure logic, easily testable
char* format_time(int hours, int minutes) {
    static char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, minutes);
    return buffer;
}

// hardware_rtc.c - Hardware-specific, test on target
void read_rtc_time(int* hours, int* minutes) {
    RTC_TimeTypeDef time;
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
    *hours = time.Hours;
    *minutes = time.Minutes;
}

// app.c - Combines both
void display_time() {
    int h, m;
    read_rtc_time(&h, &m);
    char* formatted = format_time(h, m);  // This part is tested!
    display_string(formatted);
}
```

## Sanitizers Explained

**AddressSanitizer** detects:
- Buffer overflows
- Use-after-free
- Use-after-return
- Memory leaks

**UndefinedBehaviorSanitizer** detects:
- Integer overflow
- Null pointer dereference
- Misaligned pointers
- Invalid shifts

These run automatically when you select "Run Tests with Sanitizers".

## Cppcheck Configuration

Edit `.cppcheck-suppressions` to suppress false positives:

```
// Suppress warnings for external libraries
*:tests/unity/*
*:Drivers/*

// Suppress specific warnings
unusedFunction
missingIncludeSystem
```

## Tips for Success

1. **Start small** - Test one new module at a time
2. **Test algorithms first** - Pure logic is easiest to test
3. **Run cppcheck often** - Catches issues early
4. **Use sanitizers** - They find bugs you didn't know existed
5. **Separate concerns** - Hardware vs. logic
6. **Write tests as you code** - Not after

## Example: Testing a Ring Buffer

```c
// ring_buffer.h
typedef struct {
    uint8_t buffer[256];
    uint8_t head;
    uint8_t tail;
} RingBuffer;

void rb_init(RingBuffer* rb);
bool rb_push(RingBuffer* rb, uint8_t data);
bool rb_pop(RingBuffer* rb, uint8_t* data);
bool rb_is_empty(RingBuffer* rb);
bool rb_is_full(RingBuffer* rb);

// test_ring_buffer.c
void test_push_pop() {
    RingBuffer rb;
    rb_init(&rb);
    
    TEST_ASSERT_TRUE(rb_push(&rb, 0x42));
    
    uint8_t data;
    TEST_ASSERT_TRUE(rb_pop(&rb, &data));
    TEST_ASSERT_EQUAL_HEX8(0x42, data);
}

void test_empty_buffer() {
    RingBuffer rb;
    rb_init(&rb);
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
    
    uint8_t data;
    TEST_ASSERT_FALSE(rb_pop(&rb, &data));
}

void test_full_buffer() {
    RingBuffer rb;
    rb_init(&rb);
    
    for (int i = 0; i < 256; i++) {
        TEST_ASSERT_TRUE(rb_push(&rb, i));
    }
    TEST_ASSERT_TRUE(rb_is_full(&rb));
    TEST_ASSERT_FALSE(rb_push(&rb, 0xFF));
}
```

## Next Steps

1. Identify modules with minimal HAL dependencies
2. Create tests for those modules
3. Run cppcheck on Core/Src regularly
4. Refactor tightly-coupled code over time
5. Add tests for new features before writing code (TDD)

## Resources

- Unity Documentation: `tests/unity/docs/`
- Cppcheck Manual: `man cppcheck`
- Sanitizers: https://github.com/google/sanitizers

## Need Help?

The testing infrastructure is ready. The key is writing code that separates hardware concerns from business logic. Start with modules that don't use HAL functions - those are immediately testable!
