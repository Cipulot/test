#pragma once
#include "definition_catalog.h"
#include "definition_reader.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
typedef enum { TABVIA_TAB_CONFIGURE, TABVIA_TAB_DESIGN, TABVIA_TAB_CONSOLE } tabvia_tab_t;
typedef struct tabvia_ui tabvia_ui_t;
typedef struct {
    void (*tab_changed)(void *context, tabvia_tab_t tab);
    void (*definition_selected)(void *context, const char *path, definition_version_t version);
    void (*console_clear)(void *context);
    void (*console_save)(void *context);
    void (*key_selected)(void *context, uint8_t layer, uint8_t row, uint8_t column);
    void (*keycode_apply)(void *context, uint8_t layer, uint8_t row,
                          uint8_t column, uint16_t keycode);
    void *context;
} tabvia_ui_callbacks_t;
tabvia_ui_t *tabvia_ui_create(lv_obj_t *parent, const tabvia_ui_callbacks_t *callbacks);
void tabvia_ui_set_connection(tabvia_ui_t *ui, bool connected, const char *name);
void tabvia_ui_set_definition_files(tabvia_ui_t *ui, const definition_file_t *files, size_t count);
void tabvia_ui_set_definition_result(tabvia_ui_t *ui, const definition_summary_t *summary,
                                     const definition_error_t *errors, size_t error_count);
void tabvia_ui_set_console_text(tabvia_ui_t *ui, const char *text, size_t dropped);
void tabvia_ui_set_keymap(tabvia_ui_t *ui, uint8_t layers, uint8_t rows,
                          uint8_t columns, const uint16_t *keycodes);
void tabvia_ui_set_configure_message(tabvia_ui_t *ui, const char *message);
