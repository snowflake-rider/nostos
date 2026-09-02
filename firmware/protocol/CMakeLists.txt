cmake_minimum_required(VERSION 3.16)
project(nostos_protocol C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Host address/undefined sanitizers" OFF)

add_library(nostos_protocol nostos_protocol.c nostos_uart.c)
target_include_directories(nostos_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(nostos_protocol PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

if(ENABLE_SANITIZERS)
    target_compile_options(nostos_protocol PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nostos_protocol PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
add_executable(test_protocol tests/test_protocol.c)
target_link_libraries(test_protocol PRIVATE nostos_protocol)
target_compile_options(test_protocol PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME application_protocol COMMAND test_protocol)
