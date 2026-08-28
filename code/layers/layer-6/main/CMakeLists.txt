idf_component_register(
    SRCS "main.c" "layer_packet.c" "layer_relay.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt esp_hw_support nvs_flash
)
