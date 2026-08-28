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
    ../MyApp/hw ../MyApp/service ../../../common/protocol)
target_compile_options(test_button_uart PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(ENABLE_SANITIZERS)
    target_compile_options(test_button_uart PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test_button_uart PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME button_uart COMMAND test_button_uart)
