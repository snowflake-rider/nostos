cmake_minimum_required(VERSION 3.16)
project(nostos_button_output_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

set(STM32 "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/stm32")

add_executable(test_button_output
    test_button_output.c
    ${STM32}/MyApp/ap/button_output_test.c
    ${STM32}/MyApp/hw/button.c
    ${STM32}/MyApp/hw/rgb_led.c
    ${STM32}/MyApp/hw/buzzer.c
    ${STM32}/MyApp/hw/vs1003b.c
    ${STM32}/MyApp/service/alert.c
    ${STM32}/MyApp/service/audio_service.c
    ${STM32}/MyApp/service/message_service.c
    ${STM32}/MyApp/audio/cheer_up_audio.c
    ${STM32}/MyApp/audio/rear_warning_audio.c
    ${STM32}/MyApp/audio/speed_down_request_audio.c
    ${STM32}/MyApp/audio/speed_up_request_audio.c
    ${STM32}/MyApp/audio/stop_request_audio.c
)

target_include_directories(test_button_output PRIVATE
    fakes
    ${STM32}/Core/Inc
    ${STM32}/MyApp/ap
    ${STM32}/MyApp/audio
    ${STM32}/MyApp/common
    ${STM32}/MyApp/hw
    ${STM32}/MyApp/service
    ../../libs/protocol
)
target_compile_definitions(test_button_output PRIVATE FEATURE_BUTTON_OUTPUT_TEST=1)
target_compile_options(test_button_output PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

if(ENABLE_SANITIZERS)
    target_compile_options(test_button_output PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_button_output PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME button_rgb_codec_chain COMMAND test_button_output)
