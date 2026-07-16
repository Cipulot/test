#include "tabvia_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct tabvia_ui {
    tabvia_ui_callbacks_t cb; lv_obj_t *root, *content, *status, *design_list;
    lv_obj_t *version, *errors, *console, *layer, *grid, *selection, *keycode_input, *configure_status, *keyboard;
    definition_file_t files[64]; size_t file_count; tabvia_tab_t active_tab;
    uint16_t *keycodes; uint8_t layers, rows, columns, selected_layer, selected_row, selected_column;
    char configure_message[160];
    definition_summary_t definition;
    const char *button_map[DEFINITION_MAX_LAYOUT_KEYS * 2 + 16];
    int16_t button_to_key[DEFINITION_MAX_LAYOUT_KEYS * 2];
    uint8_t button_width[DEFINITION_MAX_LAYOUT_KEYS * 2];
    char key_labels[DEFINITION_MAX_LAYOUT_KEYS][16];
};
static void style_button(lv_obj_t *o) {
    lv_obj_set_style_bg_color(o, lv_color_hex(0x221f2b), 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0x9b5cff), 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xffffff), 0);
}
static void show_panel(tabvia_ui_t *ui, tabvia_tab_t tab);
static void nav_event(lv_event_t *event) {
    tabvia_ui_t *ui = lv_event_get_user_data(event);
    tabvia_tab_t tab = (tabvia_tab_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    show_panel(ui, tab); if (ui->cb.tab_changed) ui->cb.tab_changed(ui->cb.context, tab);
}
static lv_obj_t *button(lv_obj_t *parent, const char *text, tabvia_ui_t *ui, tabvia_tab_t tab) {
    lv_obj_t *b = lv_button_create(parent); style_button(b); lv_obj_set_user_data(b, (void *)(uintptr_t)tab);
    lv_obj_add_event_cb(b, nav_event, LV_EVENT_CLICKED, ui); lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text); lv_obj_center(l); return b;
}
static void console_clear_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e); if (ui->cb.console_clear) ui->cb.console_clear(ui->cb.context);
}
static void console_save_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e); if (ui->cb.console_save) ui->cb.console_save(ui->cb.context);
}
static void keyboard_done_event(lv_event_t *e) {
    lv_obj_t *keyboard = lv_event_get_target(e);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(keyboard, NULL);
}
static void textarea_focus_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e);
    lv_keyboard_set_textarea(ui->keyboard, lv_event_get_target(e));
    lv_obj_remove_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui->keyboard);
}
static void definition_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e); uintptr_t index = (uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (index >= ui->file_count || !ui->cb.definition_selected) return;
    definition_version_t version = lv_dropdown_get_selected(ui->version) ? DEFINITION_V3 : DEFINITION_V2;
    ui->cb.definition_selected(ui->cb.context, ui->files[index].path, version);
}
static void key_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e);
    uint32_t button = lv_buttonmatrix_get_selected_button(lv_event_get_target(e));
    if (button >= DEFINITION_MAX_LAYOUT_KEYS * 2 || ui->button_to_key[button] < 0) return;
    definition_layout_key_t *key = &ui->definition.layout_keys[ui->button_to_key[button]];
    ui->selected_layer = (uint8_t)lv_dropdown_get_selected(ui->layer);
    ui->selected_row = key->row; ui->selected_column = key->column;
    size_t index = ((size_t)ui->selected_layer * ui->rows + ui->selected_row) * ui->columns + ui->selected_column;
    char text[80], code[12];
    snprintf(text, sizeof(text), "Layer %u | row %u | column %u | 0x%04X",
             ui->selected_layer, ui->selected_row, ui->selected_column, ui->keycodes[index]);
    snprintf(code, sizeof(code), "0x%04X", ui->keycodes[index]);
    lv_label_set_text(ui->selection, text); lv_textarea_set_text(ui->keycode_input, code);
    if (ui->cb.key_selected) ui->cb.key_selected(ui->cb.context, ui->selected_layer,
                                                 ui->selected_row, ui->selected_column);
}
static const char *keycode_name(uint16_t code, char *buffer, size_t size) {
    static const char *digits[] = {"1","2","3","4","5","6","7","8","9","0"};
    if (code >= 0x0004 && code <= 0x001d) { snprintf(buffer, size, "%c", 'A' + code - 0x0004); return buffer; }
    if (code >= 0x001e && code <= 0x0027) return digits[code - 0x001e];
    switch (code) {
        case 0x0000: return ""; case 0x0001: return "TRNS"; case 0x0028: return "Enter";
        case 0x0029: return "Esc"; case 0x002a: return "Bksp"; case 0x002b: return "Tab";
        case 0x002c: return "Space"; case 0x002d: return "-"; case 0x002e: return "=";
        case 0x002f: return "["; case 0x0030: return "]"; case 0x0031: return "\\";
        case 0x0033: return ";"; case 0x0034: return "'"; case 0x0035: return "`";
        case 0x0036: return ","; case 0x0037: return "."; case 0x0038: return "/";
        case 0x0039: return "Caps"; case 0x0049: return "Ins"; case 0x004a: return "Home";
        case 0x004b: return "PgUp"; case 0x004c: return "Del"; case 0x004d: return "End";
        case 0x004e: return "PgDn"; case 0x004f: return "Right"; case 0x0050: return "Left";
        case 0x0051: return "Down"; case 0x0052: return "Up";
        case 0x00e0: return "LCtrl"; case 0x00e1: return "LShift"; case 0x00e2: return "LAlt";
        case 0x00e3: return "LGUI"; case 0x00e4: return "RCtrl"; case 0x00e5: return "RShift";
        case 0x00e6: return "RAlt"; case 0x00e7: return "RGUI";
        default: snprintf(buffer, size, "%04X", code); return buffer;
    }
}
static void apply_keycode_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e); char *end = NULL;
    unsigned long value = strtoul(lv_textarea_get_text(ui->keycode_input), &end, 0);
    if (!end || *end || value > 0xffff || !ui->cb.keycode_apply) return;
    ui->cb.keycode_apply(ui->cb.context, ui->selected_layer, ui->selected_row,
                         ui->selected_column, (uint16_t)value);
}
static void render_key_grid(tabvia_ui_t *ui) {
    if (!ui->grid || !ui->keycodes || !ui->layers) return;
    ui->selected_layer = (uint8_t)lv_dropdown_get_selected(ui->layer);
    size_t map_index = 0, button_index = 0; float cursor_x = 0.0f, current_y = -1.0f;
    for (size_t i = 0; i < ui->definition.layout_key_count; i++) {
        definition_layout_key_t *key = &ui->definition.layout_keys[i];
        if (current_y < 0.0f || key->y > current_y + 0.5f) {
            if (current_y >= 0.0f) ui->button_map[map_index++] = "\n";
            current_y = key->y; cursor_x = 0.0f;
        }
        float gap = key->x - cursor_x;
        if (gap > 0.1f && button_index < DEFINITION_MAX_LAYOUT_KEYS * 2) {
            ui->button_map[map_index++] = " "; ui->button_to_key[button_index] = -1;
            ui->button_width[button_index++] = (uint8_t)(gap * 4.0f + 0.5f);
        }
        size_t code_index = ((size_t)ui->selected_layer * ui->rows + key->row) * ui->columns + key->column;
        const char *name = keycode_name(ui->keycodes[code_index], ui->key_labels[i], sizeof(ui->key_labels[i]));
        if (name != ui->key_labels[i]) snprintf(ui->key_labels[i], sizeof(ui->key_labels[i]), "%s", name);
        ui->button_map[map_index++] = ui->key_labels[i]; ui->button_to_key[button_index] = (int16_t)i;
        ui->button_width[button_index++] = (uint8_t)(key->width * 4.0f + 0.5f);
        cursor_x = key->x + key->width;
    }
    ui->button_map[map_index] = NULL; lv_buttonmatrix_set_map(ui->grid, ui->button_map);
    for (size_t i = 0; i < button_index; i++) {
        lv_buttonmatrix_set_button_width(ui->grid, i, ui->button_width[i] ? ui->button_width[i] : 1);
        if (ui->button_to_key[i] < 0) lv_buttonmatrix_set_button_ctrl(ui->grid, i, LV_BUTTONMATRIX_CTRL_DISABLED);
    }
}
static void layer_event(lv_event_t *e) { render_key_grid(lv_event_get_user_data(e)); }
static void show_panel(tabvia_ui_t *ui, tabvia_tab_t tab) {
    ui->active_tab = tab;
    if (ui->keyboard) { lv_obj_add_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN); lv_keyboard_set_textarea(ui->keyboard, NULL); }
    lv_obj_clean(ui->content);
    if (tab == TABVIA_TAB_CONFIGURE) {
        lv_obj_t *bar = lv_obj_create(ui->content); lv_obj_set_size(bar, LV_PCT(100), 70);
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
        ui->configure_status = lv_label_create(bar); lv_label_set_text(ui->configure_status, ui->configure_message[0] ? ui->configure_message : "Select a valid definition, then connect a VIA keyboard");
        ui->layer = lv_dropdown_create(bar); char options[64] = {0}; size_t used = 0;
        for (uint8_t i = 0; i < ui->layers && used < sizeof(options); i++)
            used += snprintf(options + used, sizeof(options) - used, "%sLayer %u", i ? "\n" : "", i);
        lv_dropdown_set_options(ui->layer, ui->layers ? options : "Layer 0");
        lv_obj_add_event_cb(ui->layer, layer_event, LV_EVENT_VALUE_CHANGED, ui);
        ui->selection = lv_label_create(ui->content); lv_label_set_text(ui->selection, "Tap a key to edit it");
        lv_obj_t *edit = lv_obj_create(ui->content); lv_obj_set_size(edit, LV_PCT(100), 64); lv_obj_set_flex_flow(edit, LV_FLEX_FLOW_ROW);
        ui->keycode_input = lv_textarea_create(edit); lv_textarea_set_one_line(ui->keycode_input, true); lv_obj_set_width(ui->keycode_input, 180); lv_textarea_set_text(ui->keycode_input, "0x0000");
        lv_obj_add_event_cb(ui->keycode_input, textarea_focus_event, LV_EVENT_FOCUSED, ui);
        lv_obj_t *apply = lv_button_create(edit); style_button(apply); lv_label_set_text(lv_label_create(apply), "Apply keycode"); lv_obj_add_event_cb(apply, apply_keycode_event, LV_EVENT_CLICKED, ui);
        ui->grid = lv_buttonmatrix_create(ui->content); lv_obj_set_size(ui->grid, LV_PCT(100), 390);
        lv_obj_set_scroll_dir(ui->grid, LV_DIR_ALL);
        lv_obj_add_event_cb(ui->grid, key_event, LV_EVENT_VALUE_CHANGED, ui);
        render_key_grid(ui);
    } else if (tab == TABVIA_TAB_DESIGN) {
        lv_obj_t *toolbar = lv_obj_create(ui->content); lv_obj_set_size(toolbar, LV_PCT(100), 64);
        lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
        lv_obj_t *label = lv_label_create(toolbar); lv_label_set_text(label, "Definition version");
        ui->version = lv_dropdown_create(toolbar); lv_dropdown_set_options(ui->version, "V2\nV3");
        lv_dropdown_set_selected(ui->version, 1);
        ui->design_list = lv_list_create(ui->content); lv_obj_set_size(ui->design_list, LV_PCT(100), 420);
        for (size_t i = 0; i < ui->file_count; i++) {
            lv_obj_t *item = lv_list_add_button(ui->design_list, NULL, ui->files[i].name);
            lv_obj_set_user_data(item, (void *)(uintptr_t)i);
            lv_obj_add_event_cb(item, definition_event, LV_EVENT_CLICKED, ui);
        }
        lv_obj_t *error_box = lv_obj_create(ui->content); lv_obj_set_size(error_box, LV_PCT(100), 130); lv_obj_set_scroll_dir(error_box, LV_DIR_VER);
        ui->errors = lv_label_create(error_box); lv_label_set_long_mode(ui->errors, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(ui->errors, LV_PCT(100)); lv_obj_set_style_text_color(ui->errors, lv_color_hex(0xff6b81), 0);
    } else {
        lv_obj_t *bar = lv_obj_create(ui->content); lv_obj_set_size(bar, LV_PCT(100), 60); lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
        lv_obj_t *clear = lv_button_create(bar); style_button(clear); lv_obj_add_event_cb(clear, console_clear_event, LV_EVENT_CLICKED, ui);
        lv_label_set_text(lv_label_create(clear), "Clear");
        lv_obj_t *save = lv_button_create(bar); style_button(save); lv_obj_add_event_cb(save, console_save_event, LV_EVENT_CLICKED, ui);
        lv_label_set_text(lv_label_create(save), "Save to SD");
        ui->console = lv_textarea_create(ui->content); lv_textarea_set_one_line(ui->console, false);
        lv_textarea_set_text(ui->console, ""); lv_obj_set_size(ui->console, LV_PCT(100), 480);
        lv_obj_set_style_bg_color(ui->console, lv_color_hex(0x111111), 0);
        lv_obj_set_style_text_color(ui->console, lv_color_hex(0xe6e6e6), 0);
    }
}
tabvia_ui_t *tabvia_ui_create(lv_obj_t *parent, const tabvia_ui_callbacks_t *cb) {
    tabvia_ui_t *ui = calloc(1, sizeof(*ui)); if (!ui) return NULL; if (cb) ui->cb = *cb;
    ui->root = lv_obj_create(parent); lv_obj_set_size(ui->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ui->root, lv_color_hex(0x17151d), 0); lv_obj_set_style_text_color(ui->root, lv_color_hex(0xffffff), 0);
    lv_obj_set_flex_flow(ui->root, LV_FLEX_FLOW_COLUMN); lv_obj_t *nav = lv_obj_create(ui->root);
    lv_obj_set_size(nav, LV_PCT(100), 72); lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_t *logo = lv_label_create(nav); lv_label_set_text(logo, "TabVIA"); lv_obj_set_style_text_color(logo, lv_color_hex(0xb88cff), 0);
    button(nav, "Configure", ui, TABVIA_TAB_CONFIGURE); button(nav, "Design", ui, TABVIA_TAB_DESIGN); button(nav, "HID Console", ui, TABVIA_TAB_CONSOLE);
    ui->status = lv_label_create(nav); lv_label_set_text(ui->status, "Disconnected");
    ui->content = lv_obj_create(ui->root); lv_obj_set_size(ui->content, LV_PCT(100), 620);
    ui->keyboard = lv_keyboard_create(lv_layer_top()); lv_obj_set_size(ui->keyboard, 900, 300);
    lv_obj_align(ui->keyboard, LV_ALIGN_BOTTOM_MID, 0, -8); lv_obj_add_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui->keyboard, keyboard_done_event, LV_EVENT_READY, ui);
    lv_obj_add_event_cb(ui->keyboard, keyboard_done_event, LV_EVENT_CANCEL, ui);
    lv_obj_set_flex_flow(ui->content, LV_FLEX_FLOW_COLUMN); show_panel(ui, TABVIA_TAB_CONFIGURE); return ui;
}
void tabvia_ui_set_connection(tabvia_ui_t *ui, bool connected, const char *name) {
    if (!ui) return;
    char text[128];
    snprintf(text, sizeof(text), "%s%s%s", connected ? "Connected" : "Disconnected",
             connected && name ? ": " : "", connected && name ? name : "");
    lv_label_set_text(ui->status, text);
}
void tabvia_ui_set_definition_files(tabvia_ui_t *ui, const definition_file_t *files, size_t count) {
    if (!ui) return;
    ui->file_count = count < 64 ? count : 64;
    if (ui->file_count) memcpy(ui->files, files, ui->file_count * sizeof(*files));
}
void tabvia_ui_set_definition_result(tabvia_ui_t *ui, const definition_summary_t *s,
                                     const definition_error_t *errors, size_t count) {
    if (!ui || !ui->errors) return;
    char text[1024] = {0};
    size_t used = 0;
    if (!count && s) snprintf(text, sizeof(text), "%s — %04x:%04x, %ux%u matrix, %u keys", s->name, s->vendor_id, s->product_id, s->matrix_rows, s->matrix_columns, (unsigned)s->layout_key_count);
    for (size_t i = 0; i < count && used < sizeof(text); i++) used += (size_t)snprintf(text + used, sizeof(text) - used, "%s: %s\n", errors[i].instance_path[0] ? errors[i].instance_path : "Object", errors[i].message);
    lv_label_set_text(ui->errors, text);
}
void tabvia_ui_set_console_text(tabvia_ui_t *ui, const char *text, size_t dropped) {
    if (!ui || !ui->console) return;
    lv_textarea_set_text(ui->console, text ? text : "");
    if (dropped) { char note[64]; snprintf(note, sizeof(note), "\n[%u earlier bytes dropped]", (unsigned)dropped); lv_textarea_add_text(ui->console, note); }
    lv_textarea_set_cursor_pos(ui->console, LV_TEXTAREA_CURSOR_LAST);
}
void tabvia_ui_set_keymap(tabvia_ui_t *ui, const definition_summary_t *definition,
                          uint8_t layers, const uint16_t *keycodes) {
    if (!ui || !definition || !layers || !definition->matrix_rows ||
        !definition->matrix_columns || !keycodes) return;
    size_t count = (size_t)layers * definition->matrix_rows * definition->matrix_columns;
    uint16_t *copy = realloc(ui->keycodes, count * sizeof(*copy)); if (!copy) return;
    ui->keycodes = copy; memcpy(copy, keycodes, count * sizeof(*copy));
    ui->definition = *definition; ui->layers = layers;
    ui->rows = (uint8_t)definition->matrix_rows; ui->columns = (uint8_t)definition->matrix_columns;
    if (ui->active_tab == TABVIA_TAB_CONFIGURE) show_panel(ui, TABVIA_TAB_CONFIGURE);
}
void tabvia_ui_set_configure_message(tabvia_ui_t *ui, const char *message) {
    if (!ui) return;
    snprintf(ui->configure_message, sizeof(ui->configure_message), "%s",
             message ? message : "");
    if (ui->active_tab == TABVIA_TAB_CONFIGURE && ui->configure_status)
        lv_label_set_text(ui->configure_status, ui->configure_message);
}
