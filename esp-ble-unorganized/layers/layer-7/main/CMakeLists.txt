idf_component_register(
  SRCS
    "main.c"
    "mesh_node.c"
    "serial_command.c"
  INCLUDE_DIRS "."
  REQUIRES bt nvs_flash
)
