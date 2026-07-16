idf_component_register(
  SRCS "definition_source.c" "definition_reader.c" "definition_catalog.c"
  INCLUDE_DIRS "include"
  REQUIRES json fatfs
)
