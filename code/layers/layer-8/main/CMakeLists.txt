idf_component_register(
    SRCS "main.c" "mesh_node.c" "bridge_runtime.c" "serial_command.c" "uart_diag.c"
         "../common/event_protocol.c" "../common/event_bridge.c"
    INCLUDE_DIRS "." "../common"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
