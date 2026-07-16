#pragma once
#include <stddef.h>
typedef struct { char name[128]; char path[256]; size_t size_bytes; } definition_file_t;
int definition_catalog_list(const char *directory, definition_file_t *files,
                            size_t capacity, size_t *count);
