cmake_minimum_required(VERSION 3.16)
project(redpill_class C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(REDPILL_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
add_library(redpill_modules STATIC
    bits.c
    popcount.c
    rotate.c
    checksum.c
    memmove.c
    swap.c
    matrix.c
    calculator.c
    offset.c
    pool.c
    hexdump.c
    ring_buffer.c
    linked_list.c
    bitmap.c
    min_heap.c
    timer.c
    tokenizer.c
    debounce.c
    moving_average.c
    producer_consumer.c
    uart_chat.c
)
target_include_directories(redpill_modules PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(redpill_demo main.c tests/selftest.c)
target_link_libraries(redpill_demo PRIVATE redpill_modules)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    foreach(target redpill_modules redpill_demo)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror -Wstrict-prototypes -Wmissing-prototypes)
        if(REDPILL_SANITIZE)
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endforeach()
endif()
enable_testing()
add_test(NAME boundary_checks COMMAND redpill_demo --test)
add_test(NAME all_demos COMMAND redpill_demo --all)
add_test(NAME unknown_day COMMAND redpill_demo 999)
set_tests_properties(unknown_day PROPERTIES WILL_FAIL TRUE)
add_test(NAME day4 COMMAND redpill_demo 4)
add_test(NAME day5 COMMAND redpill_demo 5)
add_test(NAME day6 COMMAND redpill_demo 6)
add_test(NAME day7 COMMAND redpill_demo 7)
add_test(NAME day8 COMMAND redpill_demo 8)
add_test(NAME day9 COMMAND redpill_demo 9)
add_test(NAME day10 COMMAND redpill_demo 10)
add_test(NAME day11 COMMAND redpill_demo 11)
add_test(NAME day12 COMMAND redpill_demo 12)
add_test(NAME day13 COMMAND redpill_demo 13)
add_test(NAME day14 COMMAND redpill_demo 14)
add_test(NAME day15 COMMAND redpill_demo 15)
add_test(NAME day16 COMMAND redpill_demo 16)
add_test(NAME day17 COMMAND redpill_demo 17)
add_test(NAME day18 COMMAND redpill_demo 18)
add_test(NAME day22 COMMAND redpill_demo 22)
add_test(NAME day23 COMMAND redpill_demo 23)
add_test(NAME day24 COMMAND redpill_demo 24)
add_test(NAME day25 COMMAND redpill_demo 25)
add_test(NAME day26 COMMAND redpill_demo 26)
add_test(NAME day33 COMMAND redpill_demo 33)
