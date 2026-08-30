set(NOSTOS_BRIDGE_SOURCE "bridge_runtime.c")
set(NOSTOS_V2_SOURCES "")
set(NOSTOS_SENSOR_SOURCES "")
if(CONFIG_NOSTOS_PROTOCOL_V2)
    set(NOSTOS_BRIDGE_SOURCE "bridge_runtime_v2.c")
    list(APPEND NOSTOS_V2_SOURCES "application_message_engine.c"
         "application_event_heap.c"
         "shared_data_cache.c"
         "official_packet_writer.c" "mesh_retry.c" "mesh_inflight.c"
         "output_command_retry.c")
endif()
if(CONFIG_NOSTOS_XOSS_SPEED_SENSOR)
    list(APPEND NOSTOS_SENSOR_SOURCES
        "xoss_ble.c" "../xoss-speed-sensor/speed_sensor.c")
endif()
idf_component_register(
    SRCS "main.c" "mesh_node.c" "${NOSTOS_BRIDGE_SOURCE}" "serial_command.c" "uart_diag.c"
         ${NOSTOS_V2_SOURCES}
         ${NOSTOS_SENSOR_SOURCES}
         "../../protocol/event_protocol.c" "../../protocol/event_bridge.c"
         "../../protocol/nostos_protocol.c" "../../protocol/nostos_uart.c"
         "../../protocol/nostos_bridge.c" "../../protocol/nostos_state.c"
         "../../protocol/nostos_debug.c" "../../protocol/sensor_link.c"
    INCLUDE_DIRS "." "../../protocol" "../xoss-speed-sensor"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
