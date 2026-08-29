cmake_minimum_required(VERSION 3.16)
project(layer8_ridar_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Host address/undefined sanitizers" OFF)

add_library(layer8_ridar_protocol
    ../main/lidar_c1_protocol.c
    ../main/serial_command.c)
target_include_directories(layer8_ridar_protocol PUBLIC ../main)
target_compile_options(layer8_ridar_protocol PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(layer8_ridar_protocol PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(layer8_ridar_protocol PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
foreach(test_name lidar_c1_protocol serial_command)
    add_executable(test_${test_name} test_${test_name}.c)
    target_link_libraries(test_${test_name} PRIVATE layer8_ridar_protocol)
    target_compile_options(test_${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME ${test_name} COMMAND test_${test_name})
endforeach()
