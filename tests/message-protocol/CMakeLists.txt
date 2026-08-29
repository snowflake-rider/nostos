cmake_minimum_required(VERSION 3.16)
project(nostos_message_protocol_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PROTOCOL "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/protocol")
add_library(nostos_protocol
    ${PROTOCOL}/nostos_protocol.c ${PROTOCOL}/nostos_uart.c
    ${PROTOCOL}/nostos_bridge.c ${PROTOCOL}/nostos_state.c ${PROTOCOL}/nostos_endpoint.c)
target_include_directories(nostos_protocol PUBLIC ${PROTOCOL})
target_compile_options(nostos_protocol PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(nostos_protocol PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nostos_protocol PUBLIC -fsanitize=address,undefined)
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mock_messages.h"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/generate_fixtures.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/mock_messages.json" "${CMAKE_CURRENT_BINARY_DIR}/mock_messages.h"
    DEPENDS mock_messages.json generate_fixtures.py VERBATIM)
add_custom_target(message_protocol_fixtures DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/mock_messages.h")
add_executable(test_message_protocol test_message_protocol.c)
add_dependencies(test_message_protocol message_protocol_fixtures)
target_include_directories(test_message_protocol PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_message_protocol PRIVATE nostos_protocol)
target_compile_options(test_message_protocol PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
enable_testing()
foreach(suite codec uart state bridge relay fuzz)
    add_test(NAME message_protocol_${suite} COMMAND test_message_protocol ${suite})
endforeach()

# Compile the actual STM32 service, GPIO/buzzer/audio services and VS1003B SPI
# driver. Only HAL is fake; MP3 assets are the actual checked-in byte arrays.
set(STM32 "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/stm32")
add_executable(test_stm32_outputs test_stm32_outputs.c
    ${STM32}/MyApp/service/uart_service.c
    ${STM32}/MyApp/service/message_router.c ${STM32}/MyApp/service/safety_service.c
    ${STM32}/MyApp/service/safety_detector.c
    ${STM32}/MyApp/service/message_protocol_service.c
    ${STM32}/MyApp/service/alert.c ${STM32}/MyApp/service/audio_service.c
    ${STM32}/MyApp/hw/rgb_led.c ${STM32}/MyApp/hw/buzzer.c ${STM32}/MyApp/hw/vs1003b.c
    ${STM32}/MyApp/audio/cheer_up_audio.c ${STM32}/MyApp/audio/rear_warning_audio.c
    ${STM32}/MyApp/audio/speed_down_request_audio.c ${STM32}/MyApp/audio/speed_up_request_audio.c
    ${STM32}/MyApp/audio/stop_request_audio.c)
target_include_directories(test_stm32_outputs PRIVATE fakes ${STM32}/Core/Inc
    ${STM32}/MyApp/service ${STM32}/MyApp/hw ${STM32}/MyApp/audio ${STM32}/MyApp/common)
target_link_libraries(test_stm32_outputs PRIVATE nostos_protocol)
target_compile_options(test_stm32_outputs PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(test_stm32_outputs PRIVATE NOSTOS_PROTOCOL_V2=1)
add_test(NAME message_protocol_stm32_outputs COMMAND test_stm32_outputs)
add_executable(test_esp32_runtime test_esp32_runtime.c)
add_dependencies(test_esp32_runtime message_protocol_fixtures)
target_include_directories(test_esp32_runtime PRIVATE fakes ../../firmware/esp32/main "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(test_esp32_runtime PRIVATE nostos_protocol)
target_compile_options(test_esp32_runtime PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME message_protocol_esp32_runtime COMMAND test_esp32_runtime)
add_library(check_stm32_v2_app OBJECT ${STM32}/MyApp/ap/app.c ${STM32}/MyApp/service/message_router.c
    ${STM32}/MyApp/service/safety_service.c)
target_include_directories(check_stm32_v2_app PRIVATE fakes ${STM32}/Core/Inc
    ${STM32}/MyApp/ap ${STM32}/MyApp/service ${STM32}/MyApp/hw ${STM32}/MyApp/common ${PROTOCOL})
target_compile_definitions(check_stm32_v2_app PRIVATE NOSTOS_PROTOCOL_V2=1)
target_compile_options(check_stm32_v2_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
