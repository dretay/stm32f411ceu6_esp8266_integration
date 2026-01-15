cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32f411ceu6_vscode_test)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Collect user application sources from Core/Src
# (excluding auto-generated files already added by stm32cubemx)
file(GLOB USER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c
)
list(REMOVE_ITEM USER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32f4xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/esp_example.c
)

# Collect library source files
file(GLOB LOGGER_SOURCES
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/rtt_logger/src/*.c
)
file(GLOB UC1698_SOURCES
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/uc1698/src/*.c
)
file(GLOB RINGBUFFER_SOURCES
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/Ring-Buffer/src/*.c
)
file(GLOB USB_MASSSTORAGE_SOURCES
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/stm32_usb_mass_storage/src/*.c
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${USER_SOURCES}
    ${UC1698_SOURCES}
    ${RINGBUFFER_SOURCES}
    ${LOGGER_SOURCES}
    ${USB_MASSSTORAGE_SOURCES}
)

# Add include paths
# IMPORTANT: Core/Inc must come BEFORE external libraries so gfxconf.h is found first
target_include_directories(${CMAKE_PROJECT_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/Ring-Buffer
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/ugfx
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/ugfx/src
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/uc1698/inc
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/rtt_logger/inc
    /Users/user/STM32CubeIDE/workspace_1.14.0/libraries/stm32_usb_mass_storage/inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    PB_FIELD_16BIT
    $<$<CONFIG:Debug>:DEBUG_LEVEL=LOG_DEBUG>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
