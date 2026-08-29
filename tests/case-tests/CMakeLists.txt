cmake_minimum_required(VERSION 3.16)
project(nostos_case_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

set(PROTOCOL "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/protocol")
add_library(case_protocol
    ${PROTOCOL}/event_protocol.c
    ${PROTOCOL}/event_bridge.c
    ${PROTOCOL}/nostos_protocol.c
    ${PROTOCOL}/nostos_uart.c
    ${PROTOCOL}/nostos_bridge.c
    ${PROTOCOL}/nostos_state.c)
target_include_directories(case_protocol PUBLIC ${PROTOCOL})
target_compile_options(case_protocol PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

if(ENABLE_SANITIZERS)
    target_compile_options(case_protocol PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(case_protocol PUBLIC -fsanitize=address,undefined)
endif()

add_executable(case_single 01-single/test_single_cases.c)
target_link_libraries(case_single PRIVATE case_protocol)
target_compile_options(case_single PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

add_executable(case_complex 02-complex/test_complex_cases.c)
target_link_libraries(case_complex PRIVATE case_protocol)
target_compile_options(case_complex PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

enable_testing()
add_test(NAME case_single COMMAND case_single)
set_tests_properties(case_single PROPERTIES LABELS single)
add_test(NAME case_complex COMMAND case_complex)
set_tests_properties(case_complex PROPERTIES LABELS complex)
