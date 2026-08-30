cmake_minimum_required(VERSION 3.16)
project(nostos_xoss_speed_sensor LANGUAGES C)

option(ENABLE_SANITIZERS
       "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(speed_sensor STATIC speed_sensor.c)
target_include_directories(speed_sensor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(speed_sensor_tests test_speed_sensor.c)
target_link_libraries(speed_sensor_tests PRIVATE speed_sensor)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    foreach(target speed_sensor speed_sensor_tests)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion)
        if(ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
        endif()
    endforeach()
endif()

enable_testing()
add_test(NAME speed_sensor_tests COMMAND speed_sensor_tests)
