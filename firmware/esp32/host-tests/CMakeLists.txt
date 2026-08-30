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
    ../main/official_packet_writer.c ../../protocol/nostos_bridge.c
    ../../protocol/nostos_protocol.c)
target_include_directories(test_v2_boot_boundary PRIVATE ../main ../../protocol)
target_compile_options(test_v2_boot_boundary PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_v2_boot_boundary PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_v2_boot_boundary PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME v2_boot_boundary COMMAND test_v2_boot_boundary)

add_executable(test_shared_data_cache test_shared_data_cache.c
    ../main/shared_data_cache.c ../../protocol/nostos_protocol.c)
target_include_directories(test_shared_data_cache PRIVATE
    ../main ../../protocol)
target_compile_options(test_shared_data_cache PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_shared_data_cache PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_shared_data_cache PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME shared_data_cache COMMAND test_shared_data_cache)

add_executable(test_official_packet_writer test_official_packet_writer.c
    ../main/official_packet_writer.c ../../protocol/nostos_protocol.c)
target_include_directories(test_official_packet_writer PRIVATE
    ../main ../../protocol)
target_compile_options(test_official_packet_writer PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_official_packet_writer PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_official_packet_writer PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME official_packet_writer COMMAND test_official_packet_writer)

add_executable(test_mesh_retry test_mesh_retry.c ../main/mesh_retry.c
    ../../protocol/nostos_protocol.c)
target_include_directories(test_mesh_retry PRIVATE ../main ../../protocol)
target_compile_options(test_mesh_retry PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_mesh_retry PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_mesh_retry PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME mesh_retry COMMAND test_mesh_retry)

add_executable(test_mesh_inflight test_mesh_inflight.c
    ../main/mesh_inflight.c ../../protocol/nostos_protocol.c)
target_include_directories(test_mesh_inflight PRIVATE ../main ../../protocol)
target_compile_options(test_mesh_inflight PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_mesh_inflight PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_mesh_inflight PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME mesh_inflight COMMAND test_mesh_inflight)

add_executable(test_remote_event_backlog test_remote_event_backlog.c
    ../main/remote_event_backlog.c ../../protocol/nostos_protocol.c)
target_include_directories(test_remote_event_backlog PRIVATE
    ../main ../../protocol)
target_compile_options(test_remote_event_backlog PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_remote_event_backlog PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_remote_event_backlog PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME remote_event_backlog COMMAND test_remote_event_backlog)

add_executable(test_application_message_engine
    test_application_message_engine.c
    ../main/application_message_engine.c
    ../main/application_event_heap.c
    ../main/official_packet_writer.c
    ../main/shared_data_cache.c
    ../../protocol/nostos_bridge.c
    ../../protocol/nostos_protocol.c
    ../../protocol/nostos_state.c
    ../../protocol/sensor_link.c)
target_include_directories(test_application_message_engine PRIVATE
    ../main ../../protocol)
target_compile_options(test_application_message_engine PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_application_message_engine PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_application_message_engine PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME application_message_engine COMMAND test_application_message_engine)

add_executable(test_application_event_heap test_application_event_heap.c
    ../main/application_event_heap.c ../../protocol/nostos_protocol.c)
target_include_directories(test_application_event_heap PRIVATE
    ../main ../../protocol)
target_compile_options(test_application_event_heap PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_application_event_heap PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_application_event_heap PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME application_event_heap COMMAND test_application_event_heap)

add_executable(test_nostos_project_scenario
    test_nostos_project_scenario.c
    ../main/application_message_engine.c
    ../main/application_event_heap.c
    ../main/output_command_retry.c
    ../main/official_packet_writer.c
    ../main/shared_data_cache.c
    ../../protocol/nostos_bridge.c
    ../../protocol/nostos_protocol.c
    ../../protocol/nostos_state.c
    ../../protocol/sensor_link.c)
target_include_directories(test_nostos_project_scenario PRIVATE
    ../main ../../protocol)
target_compile_options(test_nostos_project_scenario PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_nostos_project_scenario PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_nostos_project_scenario PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME nostos_project_complex_scenario COMMAND test_nostos_project_scenario)

target_sources(test_serial_command PRIVATE ../main/serial_command.c)
target_include_directories(test_serial_command PRIVATE ../main)
