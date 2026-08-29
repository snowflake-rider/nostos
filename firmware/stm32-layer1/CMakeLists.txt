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
option(NOSTOS_PROTOCOL_V2 "Use v2 endpoint; requires trusted boot/session provider" OFF)
option(BUTTON_OUTPUT_TEST "Diagnostic button -> RGB -> VS1003B output mode" OFF)
if(BUTTON_OUTPUT_TEST AND NOSTOS_PROTOCOL_V2)
    message(FATAL_ERROR "BUTTON_OUTPUT_TEST currently exercises the legacy button message path; disable NOSTOS_PROTOCOL_V2")
endif()
if(NOSTOS_PROTOCOL_V2)
    add_compile_definitions(NOSTOS_PROTOCOL_V2=1)
endif()
if(BUTTON_OUTPUT_TEST)
    add_compile_definitions(
        FEATURE_BUTTON_OUTPUT_TEST=1
        FEATURE_ULTRASONIC_SENSOR=0
        FEATURE_FALL_DETECTION=0
    )
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
    ../../libs/protocol/nostos_protocol.c
    ../../libs/protocol/nostos_uart.c
    ../../libs/protocol/nostos_state.c
    ../../libs/protocol/nostos_endpoint.c
    ../../libs/protocol/nostos_debug.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    MyApp/ap
    MyApp/audio
    MyApp/common
    MyApp/hw
    MyApp/service
    ../../libs/protocol
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
