set(NOSTOS_BRIDGE_SOURCE "bridge_runtime.c")
if(CONFIG_NOSTOS_PROTOCOL_V2)
    set(NOSTOS_BRIDGE_SOURCE "bridge_runtime_v2.c")
endif()
idf_component_register(
    SRCS "main.c" "mesh_node.c" "${NOSTOS_BRIDGE_SOURCE}" "serial_command.c" "uart_diag.c"
         "../../../libs/protocol/event_protocol.c" "../../../libs/protocol/event_bridge.c"
         "../../../libs/protocol/nostos_protocol.c" "../../../libs/protocol/nostos_uart.c"
         "../../../libs/protocol/nostos_bridge.c"
    INCLUDE_DIRS "." "../../../libs/protocol"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
