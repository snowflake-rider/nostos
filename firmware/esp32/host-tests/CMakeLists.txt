cmake_minimum_required(VERSION 3.16)
project(layer8_host_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Host address/undefined sanitizers" OFF)

add_library(event_protocol ../../protocol/event_protocol.c ../../protocol/event_bridge.c)
target_include_directories(event_protocol PUBLIC ../../protocol)
target_compile_options(event_protocol PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(event_protocol PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(event_protocol PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
add_executable(test_event_path test_event_path.c)
target_link_libraries(test_event_path PRIVATE event_protocol)
target_compile_options(test_event_path PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME event_path COMMAND test_event_path)

foreach(test_name event_protocol event_bridge serial_command)
    add_executable(test_${test_name} test_${test_name}.c)
    target_link_libraries(test_${test_name} PRIVATE event_protocol)
    target_compile_options(test_${test_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME ${test_name} COMMAND test_${test_name})
endforeach()

add_executable(test_v2_boot_boundary test_v2_boot_boundary.c
    ../../protocol/nostos_bridge.c ../../protocol/nostos_protocol.c)
target_include_directories(test_v2_boot_boundary PRIVATE ../../protocol)
target_compile_options(test_v2_boot_boundary PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_v2_boot_boundary PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_v2_boot_boundary PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME v2_boot_boundary COMMAND test_v2_boot_boundary)

target_sources(test_serial_command PRIVATE ../main/serial_command.c)
target_include_directories(test_serial_command PRIVATE ../main)
