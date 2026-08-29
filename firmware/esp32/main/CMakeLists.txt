set(NOSTOS_BRIDGE_SOURCE "bridge_runtime.c")
if(CONFIG_NOSTOS_PROTOCOL_V2)
    set(NOSTOS_BRIDGE_SOURCE "bridge_runtime_v2.c")
endif()
idf_component_register(
    SRCS "main.c" "mesh_node.c" "${NOSTOS_BRIDGE_SOURCE}" "serial_command.c" "uart_diag.c"
         "../../protocol/event_protocol.c" "../../protocol/event_bridge.c"
         "../../protocol/nostos_protocol.c" "../../protocol/nostos_uart.c"
         "../../protocol/nostos_bridge.c"
         "../../protocol/nostos_debug.c"
    INCLUDE_DIRS "." "../../protocol"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
