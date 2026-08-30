cmake_minimum_required(VERSION 3.16)
project(stm32_shared_state_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Enable host address/undefined sanitizers" OFF)

enable_testing()
add_library(sensor_store ../MyApp/service/sensor_store.c)
target_include_directories(sensor_store PUBLIC ../MyApp/service)
target_compile_options(sensor_store PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(sensor_store PUBLIC m)
if(ENABLE_SANITIZERS)
    target_compile_options(sensor_store PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sensor_store PUBLIC -fsanitize=address,undefined)
endif()
add_executable(test_sensor_store test_sensor_store.c)
target_link_libraries(test_sensor_store PRIVATE sensor_store)
target_compile_options(test_sensor_store PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME sensor_store COMMAND test_sensor_store)

add_library(sensor_view_service ../MyApp/service/sensor_view_service.c)
target_include_directories(sensor_view_service PUBLIC
    ../MyApp/service ../../protocol)
target_compile_options(sensor_view_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(sensor_view_service PUBLIC sensor_store)
if(ENABLE_SANITIZERS)
    target_compile_options(sensor_view_service PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sensor_view_service PUBLIC -fsanitize=address,undefined)
endif()
add_executable(test_sensor_view_service test_sensor_view_service.c)
target_link_libraries(test_sensor_view_service PRIVATE sensor_view_service)
target_compile_options(test_sensor_view_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
add_test(NAME sensor_view_service COMMAND test_sensor_view_service)

add_executable(test_sensor_sync_service test_sensor_sync_service.c
    ../MyApp/service/sensor_sync_service.c)
target_include_directories(test_sensor_sync_service PRIVATE fakes ../Core/Inc
    ../MyApp/service ../MyApp/hw ../../protocol)
target_compile_definitions(test_sensor_sync_service PRIVATE NOSTOS_PROTOCOL_V2=1)
target_compile_options(test_sensor_sync_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_sensor_sync_service PRIVATE sensor_store)
if(ENABLE_SANITIZERS)
    target_compile_options(test_sensor_sync_service PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_sensor_sync_service PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME sensor_sync_service COMMAND test_sensor_sync_service)

add_executable(test_message_protocol_identity
    test_message_protocol_identity.c
    ../MyApp/service/message_protocol_service.c
    ../../protocol/sensor_link.c
    ../../protocol/nostos_protocol.c
    ../../protocol/nostos_uart.c
    ../../protocol/nostos_state.c
    ../../protocol/nostos_endpoint.c)
target_include_directories(test_message_protocol_identity PRIVATE
    fakes ../Core/Inc ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_options(test_message_protocol_identity PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/message_protocol_test_platform.h
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_message_protocol_identity PRIVATE sensor_view_service)
if(ENABLE_SANITIZERS)
    target_compile_options(test_message_protocol_identity PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_message_protocol_identity PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME message_protocol_identity COMMAND test_message_protocol_identity)

# Exercise the real button/USART service APIs; only the hardware HAL is replaced.
add_executable(test_button_uart test_button_uart.c
    ../MyApp/hw/button.c ../MyApp/service/uart_service.c)
target_include_directories(test_button_uart PRIVATE fakes ../Core/Inc
    ../MyApp/hw ../MyApp/service ../../protocol)
target_compile_options(test_button_uart PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_button_uart PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_button_uart PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME button_uart COMMAND test_button_uart)

add_executable(test_message_outputs test_message_outputs.c
    ../MyApp/service/message_service.c ../MyApp/service/alert.c
    ../MyApp/hw/rgb_led.c ../MyApp/hw/buzzer.c)
target_include_directories(test_message_outputs PRIVATE fakes ../Core/Inc
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_options(test_message_outputs PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_message_outputs PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_message_outputs PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME message_outputs COMMAND test_message_outputs)

add_executable(test_calibration_audio_asset test_calibration_audio_asset.c
    ../MyApp/audio/calibration_completed_audio.c)
target_include_directories(test_calibration_audio_asset PRIVATE ../MyApp/audio)
target_compile_options(test_calibration_audio_asset PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_calibration_audio_asset PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_calibration_audio_asset PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME calibration_audio_asset COMMAND test_calibration_audio_asset)

add_executable(test_safety_calibration test_safety_calibration.c
    ../MyApp/service/safety_detector.c)
target_include_directories(test_safety_calibration PRIVATE
    ../MyApp/service ../MyApp/common ../../protocol)
target_compile_options(test_safety_calibration PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_safety_calibration PRIVATE m)
if(ENABLE_SANITIZERS)
    target_compile_options(test_safety_calibration PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_safety_calibration PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME safety_calibration COMMAND test_safety_calibration)

add_executable(test_safety_service test_safety_service.c
    ../MyApp/service/safety_service.c ../MyApp/service/safety_detector.c)
target_include_directories(test_safety_service PRIVATE fakes ../Core/Inc
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_definitions(test_safety_service PRIVATE
    FEATURE_FALL_DETECTION=1 NOSTOS_PROTOCOL_V2=0)
target_compile_options(test_safety_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_safety_service PRIVATE sensor_store m)
if(ENABLE_SANITIZERS)
    target_compile_options(test_safety_service PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_safety_service PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME safety_service COMMAND test_safety_service)

add_executable(test_environment_service test_environment_service.c
    ../MyApp/service/environment_service.c)
target_include_directories(test_environment_service PRIVATE fakes
    ../MyApp/service ../MyApp/hw ../MyApp/common)
target_compile_definitions(test_environment_service PRIVATE
    FEATURE_DHT11_SENSOR=1)
target_compile_options(test_environment_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_environment_service PRIVATE sensor_store)
if(ENABLE_SANITIZERS)
    target_compile_options(test_environment_service PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_environment_service PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME environment_service COMMAND test_environment_service)

add_executable(test_display_service test_display_service.c
    ../MyApp/service/display_service.c)
target_include_directories(test_display_service PRIVATE fakes
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_definitions(test_display_service PRIVATE
    FEATURE_SSD1306_DISPLAY=1 FEATURE_DHT11_SENSOR=1)
target_compile_options(test_display_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_display_service PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_display_service PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME display_service COMMAND test_display_service)

add_executable(test_display_no_dht test_display_service.c
    ../MyApp/service/display_service.c)
target_include_directories(test_display_no_dht PRIVATE fakes
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_definitions(test_display_no_dht PRIVATE
    FEATURE_SSD1306_DISPLAY=1 FEATURE_DHT11_SENSOR=0)
target_compile_options(test_display_no_dht PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_display_no_dht PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_display_no_dht PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME display_no_dht COMMAND test_display_no_dht)

add_executable(test_ssd1306 test_ssd1306.c ../MyApp/hw/ssd1306.c)
target_include_directories(test_ssd1306 PRIVATE fakes
    ../MyApp/hw ../MyApp/common)
target_compile_options(test_ssd1306 PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_ssd1306 PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_ssd1306 PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME ssd1306 COMMAND test_ssd1306)

# Compile the actual services with the connected board's sensorless policy.
add_library(check_sensorless_services OBJECT
    ../MyApp/service/environment_service.c
    ../MyApp/service/safety_service.c)
target_include_directories(check_sensorless_services PRIVATE fakes ../Core/Inc
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_definitions(check_sensorless_services PRIVATE
    FEATURE_DHT11_SENSOR=0 FEATURE_FALL_DETECTION=0
    NOSTOS_PROTOCOL_V2=0)
target_compile_options(check_sensorless_services PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)

# Compile the real app integration that consumes the BTN4 local request.
add_library(check_calibration_app OBJECT ../MyApp/ap/app.c)
target_include_directories(check_calibration_app PRIVATE fakes ../Core/Inc
    ../MyApp/ap ../MyApp/service ../MyApp/hw ../MyApp/audio ../MyApp/common
    ../../protocol)
target_compile_definitions(check_calibration_app PRIVATE
    NOSTOS_PROTOCOL_V2=0)
target_compile_options(check_calibration_app PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
