#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { DEFINITION_V2 = 2, DEFINITION_V3 = 3 } definition_version_t;
typedef enum { DEFINITION_SHAPE_KEYBOARD, DEFINITION_SHAPE_VIA } definition_shape_t;
#define DEFINITION_MAX_LAYOUT_KEYS 160
typedef struct {
    float x, y, width, height;
    uint8_t row, column;
    uint32_t color;
} definition_layout_key_t;
typedef struct {
    definition_version_t version; definition_shape_t shape; char name[96];
    uint16_t vendor_id, product_id, matrix_rows, matrix_columns;
    uint32_t vendor_product_id; size_t layout_key_count;
    bool has_menus, has_custom_keycodes;
    definition_layout_key_t layout_keys[DEFINITION_MAX_LAYOUT_KEYS];
} definition_summary_t;
typedef struct { char instance_path[128]; char message[160]; } definition_error_t;
bool definition_reader_validate(const char *json, definition_version_t selected_version,
    definition_summary_t *out, definition_error_t *errors,
    size_t error_capacity, size_t *error_count);
bool definition_reader_validate_file(const char *path,
    definition_version_t selected_version, definition_summary_t *out,
    definition_error_t *errors, size_t error_capacity, size_t *error_count);
