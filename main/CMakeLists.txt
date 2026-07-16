idf_component_register(
  SRCS "app_main.c"
  INCLUDE_DIRS "."
  REQUIRES via_protocol via_transport hid_console platform_tab5 ui
           definitions m5stack_tab5 esp_timer heap
)
