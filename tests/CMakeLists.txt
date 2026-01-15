cmake_minimum_required(VERSION 3.22)

# This is for HOST testing (running on macOS, not STM32)
# We compile the application code natively for testing

project(stm32f411ceu6_tests C)

# Enable testing
enable_testing()

# Add Unity test framework
add_library(unity STATIC
    unity/src/unity.c
)
target_include_directories(unity PUBLIC
    unity/src
)

# Add STM32 definitions for header compatibility
add_compile_definitions(
    USE_HAL_DRIVER
    STM32F411xE
)

# Override problematic macros for host compilation
add_compile_definitions(__RAM_FUNC=)

# Compiler flags for host tests with sanitizers
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")

# Mock STM32 HAL functions for host testing
add_library(stm32_mocks STATIC
    mocks/stm32_hal_mock.c
)
target_include_directories(stm32_mocks PUBLIC
    mocks
    ../Core/Inc
    ../Drivers/STM32F4xx_HAL_Driver/Inc
    ../Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ../Drivers/CMSIS/Include
)

# Example test executable (DISABLED - DateHelper has HAL dependencies)
# To test HAL-dependent code, refactor to separate business logic from HAL calls
# See tests/README.md for examples
#
# add_executable(test_datehelper
#     test_datehelper.c
#     ../Core/Src/DateHelper.c
# )
# target_include_directories(test_datehelper PRIVATE
#     ../Core/Inc
#     ../Drivers/STM32F4xx_HAL_Driver/Inc
#     ../Drivers/CMSIS/Device/ST/STM32F4xx/Include
#     ../Drivers/CMSIS/Include
# )
# target_link_libraries(test_datehelper
#     unity
#     stm32_mocks
# )
# add_test(NAME DateHelper COMMAND test_datehelper)

# String utilities test (no HAL dependencies - works!)
add_executable(test_string_utils
    test_string_utils.c
)
target_include_directories(test_string_utils PRIVATE
    ../Core/Inc
)
target_link_libraries(test_string_utils unity)
add_test(NAME StringUtils COMMAND test_string_utils)

# Add more test executables here...
