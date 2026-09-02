set(NOSTOS_SENSOR_SOURCES "")
if(CONFIG_NOSTOS_XOSS_SPEED_SENSOR)
    list(APPEND NOSTOS_SENSOR_SOURCES
        "xoss_ble.c" "../xoss-speed-sensor/speed_sensor.c")
endif()
idf_component_register(
    SRCS "main.c" "mesh_node.c" "bridge_runtime.c"
         "application_runtime.c" "serial_command.c"
         ${NOSTOS_SENSOR_SOURCES}
         "../../protocol/nostos_protocol.c" "../../protocol/nostos_uart.c"
    INCLUDE_DIRS "." "../../protocol" "../xoss-speed-sensor"
    REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_timer example_init)
