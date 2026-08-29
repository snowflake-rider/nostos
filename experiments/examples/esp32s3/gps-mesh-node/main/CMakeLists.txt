idf_component_register(
    SRCS "main.c" "mesh_node.c" "serial_command.c" "gps_mesh.c" "gps_codec.c" "gps_receiver.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash esp_timer)
