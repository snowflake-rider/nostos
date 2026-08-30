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
set(CMAKE_PROJECT_NAME nostos_stm32)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
option(NOSTOS_PROTOCOL_V2 "Use v2 endpoint; requires trusted boot/session provider" ON)
option(NOSTOS_FREERTOS "Run the STM32 application on FreeRTOS" ON)
option(BUTTON_OUTPUT_TEST "Diagnostic button -> RGB -> VS1003B output mode" OFF)
option(SSD1306_DISPLAY "Enable SSD1306 display on I2C1 PB8/PB9" ON)
option(MPU6050_SENSOR "Enable MPU6050 fall detection on I2C1 PB8/PB9" OFF)
option(DHT11_SENSOR "Enable local DHT11 sampling on PA1" OFF)
if(BUTTON_OUTPUT_TEST AND NOSTOS_PROTOCOL_V2)
    message(FATAL_ERROR "BUTTON_OUTPUT_TEST currently exercises the legacy button message path; disable NOSTOS_PROTOCOL_V2")
endif()
if(NOSTOS_PROTOCOL_V2)
    add_compile_definitions(NOSTOS_PROTOCOL_V2=1)
endif()
if(NOSTOS_FREERTOS)
    add_compile_definitions(NOSTOS_FREERTOS=1)
else()
    add_compile_definitions(NOSTOS_FREERTOS=0)
endif()
if(BUTTON_OUTPUT_TEST)
    set(SSD1306_DISPLAY OFF)
    set(MPU6050_SENSOR OFF)
    set(DHT11_SENSOR OFF)
    add_compile_definitions(
        FEATURE_BUTTON_OUTPUT_TEST=1
    )
endif()
if(SSD1306_DISPLAY)
    add_compile_definitions(FEATURE_SSD1306_DISPLAY=1)
else()
    add_compile_definitions(FEATURE_SSD1306_DISPLAY=0)
endif()
if(MPU6050_SENSOR)
    add_compile_definitions(FEATURE_FALL_DETECTION=1)
else()
    add_compile_definitions(FEATURE_FALL_DETECTION=0)
endif()
if(DHT11_SENSOR)
    add_compile_definitions(FEATURE_DHT11_SENSOR=1)
else()
    add_compile_definitions(FEATURE_DHT11_SENSOR=0)
endif()
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})
if(NOSTOS_PROTOCOL_V2)
    # The default boot hook fails closed. Still retain the public v2 init entry
    # and its output/codec dependencies for trusted deployment integration.
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,--undefined=message_protocol_service_init")
endif()

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

if(NOSTOS_FREERTOS)
    include(FetchContent)
    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/MyApp/rtos
    )
    set(FREERTOS_PORT GCC_ARM_CM4F CACHE STRING "FreeRTOS ARM Cortex-M4F port" FORCE)
    FetchContent_Declare(freertos_kernel
        URL https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/3a22924e0a9ddbbc8b0758881c33b3422a5cc20d.tar.gz
        URL_HASH SHA256=e7d25915c043f4695eb066117cc60a42e7c58e421d971ebaf6fe2ec97208d658
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(freertos_kernel)
endif()

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# MyApp 아래의 모든 C 소스를 자동 검색
file(GLOB_RECURSE MYAPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/MyApp/*.c"
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${MYAPP_SOURCES}
    ../protocol/nostos_protocol.c
    ../protocol/nostos_uart.c
    ../protocol/nostos_state.c
    ../protocol/nostos_endpoint.c
    ../protocol/nostos_debug.c
    ../protocol/sensor_link.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    MyApp/ap
    MyApp/audio
    MyApp/common
    MyApp/hw
    MyApp/rtos
    MyApp/service
    ../protocol
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
if(NOSTOS_FREERTOS)
    target_link_libraries(${CMAKE_PROJECT_NAME} freertos_kernel)
endif()
