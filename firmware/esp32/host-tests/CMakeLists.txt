cmake_minimum_required(VERSION 3.16)
project(nostos_esp32_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Host address/undefined sanitizers" OFF)

add_library(nostos_protocol
    ../../protocol/nostos_protocol.c
    ../../protocol/nostos_uart.c)
target_include_directories(nostos_protocol PUBLIC ../../protocol)
target_compile_options(nostos_protocol PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

if(ENABLE_SANITIZERS)
    target_compile_options(nostos_protocol PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nostos_protocol PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()

add_executable(test_application_runtime
    test_application_runtime.c
    ../main/application_runtime.c)
target_include_directories(test_application_runtime PRIVATE
    ../main ../../protocol)
target_link_libraries(test_application_runtime PRIVATE nostos_protocol)
target_compile_options(test_application_runtime PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME application_runtime COMMAND test_application_runtime)

add_executable(test_serial_command
    test_serial_command.c
    ../main/serial_command.c)
target_include_directories(test_serial_command PRIVATE ../main)
target_compile_options(test_serial_command PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME serial_command COMMAND test_serial_command)

add_executable(test_xoss_speed_sensor
    ../xoss-speed-sensor/test_speed_sensor.c
    ../xoss-speed-sensor/speed_sensor.c)
target_include_directories(test_xoss_speed_sensor PRIVATE
    ../xoss-speed-sensor ../main)
target_compile_options(test_xoss_speed_sensor PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME xoss_speed_sensor COMMAND test_xoss_speed_sensor)

if(ENABLE_SANITIZERS)
    foreach(target_name
            test_application_runtime test_serial_command test_xoss_speed_sensor)
        target_compile_options(${target_name} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE
            -fsanitize=address,undefined)
    endforeach()
endif()
