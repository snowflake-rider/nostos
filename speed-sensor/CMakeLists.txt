cmake_minimum_required(VERSION 3.16)
project(nostos_speed_sensor_prototype LANGUAGES C)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(speed_sensor STATIC
    src/xoss_csc.c
    src/speed_sensor_local.c
)
target_include_directories(speed_sensor PUBLIC include)
target_compile_options(speed_sensor PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion)

add_executable(speed_sensor_demo examples/demo.c)
target_link_libraries(speed_sensor_demo PRIVATE speed_sensor)
target_compile_options(speed_sensor_demo PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion)

enable_testing()
add_executable(speed_sensor_tests
    tests/test_speed_sensor.c
    ../libs/protocol/nostos_uart.c
)
target_include_directories(speed_sensor_tests PRIVATE ../libs/protocol)
target_link_libraries(speed_sensor_tests PRIVATE speed_sensor)
target_compile_options(speed_sensor_tests PRIVATE
    -Wall -Wextra -Werror -Wpedantic -Wconversion -Wsign-conversion)
add_test(NAME speed_sensor_tests COMMAND speed_sensor_tests)

if(ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    foreach(target speed_sensor speed_sensor_demo speed_sensor_tests)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
endif()
