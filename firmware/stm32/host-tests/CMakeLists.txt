cmake_minimum_required(VERSION 3.16)
project(stm32_shared_state_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Enable host address/undefined sanitizers" OFF)

add_library(shared_state ../MyApp/service/shared_state.c)
target_include_directories(shared_state PUBLIC ../MyApp/service)
target_compile_options(shared_state PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(shared_state PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(shared_state PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
add_executable(test_shared_state test_shared_state.c)
target_link_libraries(test_shared_state PRIVATE shared_state)
target_compile_options(test_shared_state PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME shared_state COMMAND test_shared_state)

add_executable(shared_state_example shared_state_example.c)
target_link_libraries(shared_state_example PRIVATE shared_state)
target_compile_options(shared_state_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME shared_state_example COMMAND shared_state_example)

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
    FEATURE_ULTRASONIC_SENSOR=0 FEATURE_FALL_DETECTION=1
    NOSTOS_PROTOCOL_V2=0)
target_compile_options(test_safety_service PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
target_link_libraries(test_safety_service PRIVATE m)
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
    ../MyApp/service ../MyApp/hw ../MyApp/common)
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
    ../MyApp/service ../MyApp/hw ../MyApp/common)
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

# Compile the actual services with the connected board's sensorless policy.
add_library(check_sensorless_services OBJECT
    ../MyApp/service/environment_service.c
    ../MyApp/service/safety_service.c)
target_include_directories(check_sensorless_services PRIVATE fakes ../Core/Inc
    ../MyApp/service ../MyApp/hw ../MyApp/common ../../protocol)
target_compile_definitions(check_sensorless_services PRIVATE
    FEATURE_DHT11_SENSOR=0 FEATURE_FALL_DETECTION=0
    FEATURE_ULTRASONIC_SENSOR=0 NOSTOS_PROTOCOL_V2=0)
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
