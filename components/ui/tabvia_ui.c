#include "tabvia_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct tabvia_ui {
    tabvia_ui_callbacks_t cb; lv_obj_t *root, *content, *status, *design_list;
    lv_obj_t *version, *errors, *console; definition_file_t files[64]; size_t file_count;
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
static void definition_event(lv_event_t *e) {
    tabvia_ui_t *ui = lv_event_get_user_data(e); uintptr_t index = (uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (index >= ui->file_count || !ui->cb.definition_selected) return;
    definition_version_t version = lv_dropdown_get_selected(ui->version) ? DEFINITION_V3 : DEFINITION_V2;
    ui->cb.definition_selected(ui->cb.context, ui->files[index].path, version);
}
static void show_panel(tabvia_ui_t *ui, tabvia_tab_t tab) {
    lv_obj_clean(ui->content);
    if (tab == TABVIA_TAB_CONFIGURE) {
        lv_obj_t *title = lv_label_create(ui->content); lv_label_set_text(title, "CONFIGURE");
        lv_obj_t *hint = lv_label_create(ui->content); lv_label_set_text(hint, "Connect a VIA keyboard to USB-A");
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
        ui->errors = lv_label_create(ui->content); lv_label_set_long_mode(ui->errors, LV_LABEL_LONG_WRAP);
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
