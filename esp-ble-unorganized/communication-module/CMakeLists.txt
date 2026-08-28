cmake_minimum_required(VERSION 3.16)
project(comm_module_host VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(COMM_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan for host tests" OFF)

add_library(comm_host_options INTERFACE)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(comm_host_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
    if(COMM_ENABLE_SANITIZERS)
        target_compile_options(comm_host_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(comm_host_options INTERFACE -fsanitize=address,undefined)
    endif()
elseif(COMM_ENABLE_SANITIZERS)
    message(FATAL_ERROR "COMM_ENABLE_SANITIZERS requires Clang or GCC")
endif()

add_library(comm_common INTERFACE)
target_include_directories(comm_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/common")

add_library(comm_event_queue STATIC event-driven/comm_event_queue.c)
target_include_directories(comm_event_queue PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/event-driven")
target_link_libraries(comm_event_queue PUBLIC comm_common PRIVATE comm_host_options)

enable_testing()
add_executable(test_event_queue event-driven/tests/test_event_queue.c)
target_link_libraries(test_event_queue PRIVATE comm_event_queue comm_host_options)
add_test(NAME event_queue COMMAND test_event_queue)

add_executable(event_queue_demo event-driven/examples/main.c)
target_link_libraries(event_queue_demo PRIVATE comm_event_queue comm_host_options)
add_test(NAME event_queue_demo COMMAND event_queue_demo)

add_library(comm_moving_average STATIC periodic/moving_average.c)
target_include_directories(comm_moving_average PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/periodic")
target_link_libraries(comm_moving_average PRIVATE comm_host_options)

add_executable(test_moving_average periodic/tests/test_moving_average.c)
target_link_libraries(test_moving_average PRIVATE comm_moving_average comm_host_options)
if(UNIX)
    target_link_libraries(test_moving_average PRIVATE m)
endif()
add_test(NAME moving_average COMMAND test_moving_average)

add_library(comm_periodic STATIC periodic/comm_periodic.c)
target_include_directories(comm_periodic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/periodic")
target_link_libraries(comm_periodic PUBLIC comm_common comm_moving_average
    PRIVATE comm_host_options)

add_executable(test_periodic periodic/tests/test_periodic.c)
target_link_libraries(test_periodic PRIVATE comm_periodic comm_host_options)
if(UNIX)
    target_link_libraries(test_periodic PRIVATE m)
endif()
add_test(NAME periodic COMMAND test_periodic)

add_executable(periodic_demo periodic/examples/main.c)
target_link_libraries(periodic_demo PRIVATE comm_periodic comm_host_options)
if(UNIX)
    target_link_libraries(periodic_demo PRIVATE m)
endif()
add_test(NAME periodic_demo COMMAND periodic_demo)

add_library(comm_service STATIC service/comm.c)
target_include_directories(comm_service PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/service")
target_link_libraries(comm_service PUBLIC comm_event_queue comm_periodic
    PRIVATE comm_host_options)

add_executable(test_comm service/tests/test_comm.c)
target_link_libraries(test_comm PRIVATE comm_service comm_host_options)
if(UNIX)
    target_link_libraries(test_comm PRIVATE m)
endif()
add_test(NAME comm_service COMMAND test_comm)

add_executable(comm_service_demo service/examples/main.c)
target_link_libraries(comm_service_demo PRIVATE comm_service comm_host_options)
if(UNIX)
    target_link_libraries(comm_service_demo PRIVATE m)
endif()
add_test(NAME comm_service_demo COMMAND comm_service_demo)
