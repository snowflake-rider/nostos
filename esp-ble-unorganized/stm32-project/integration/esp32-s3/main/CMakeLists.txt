idf_component_register(
    SRCS "main.c" "mesh_node.c" "bridge_runtime.c" "serial_command.c"
         "../../../common/protocol/event_protocol.c"
         "../../../common/protocol/event_bridge.c"
    INCLUDE_DIRS "." "../../../common/protocol"
    REQUIRES bt nvs_flash esp_driver_uart esp_timer example_init)
