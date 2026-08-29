set(LAYER8_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../main")
set(PROTOCOL_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../libs/protocol")

set(NOSTOS_BRIDGE_SOURCE "${LAYER8_MAIN_DIR}/bridge_runtime.c")
if(CONFIG_NOSTOS_PROTOCOL_V2)
    set(NOSTOS_BRIDGE_SOURCE "${LAYER8_MAIN_DIR}/bridge_runtime_v2.c")
endif()

idf_component_register(
    SRCS "main.c" "serial_command.c" "lidar_c1.c" "lidar_c1_protocol.c"
         "${LAYER8_MAIN_DIR}/mesh_node.c"
         "${NOSTOS_BRIDGE_SOURCE}"
         "${LAYER8_MAIN_DIR}/uart_diag.c"
         "${PROTOCOL_DIR}/event_protocol.c"
         "${PROTOCOL_DIR}/event_bridge.c"
         "${PROTOCOL_DIR}/nostos_protocol.c"
         "${PROTOCOL_DIR}/nostos_uart.c"
         "${PROTOCOL_DIR}/nostos_bridge.c"
    INCLUDE_DIRS "." "${LAYER8_MAIN_DIR}" "${PROTOCOL_DIR}"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
