#include <stdio.h>
#include <string.h>
#include "bsp/m5stack_tab5.h"
#include "definition_catalog.h"
#include "definition_reader.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hid_console.h"
#include "tabvia_ui.h"
#include "tabvia_usb.h"
#include "via_protocol.h"
#include "via_transport.h"

static const char *TAG = "tabvia";
static tabvia_ui_t *s_ui;
static via_transport_t s_transport;
static hid_console_t s_console;
static char *s_console_storage;
static definition_summary_t s_definition;
static bool s_definition_valid;
static volatile bool s_keyboard_connected, s_sync_requested, s_response_ready;
static volatile bool s_write_requested;
static uint16_t s_connected_vid, s_connected_pid;
static uint16_t *s_keycodes;
static uint8_t s_layers, s_read_layer, s_read_row, s_read_column;
static uint8_t s_write_layer, s_write_row, s_write_column;
static uint16_t s_write_keycode;
static via_report_t s_response;
typedef enum { SYNC_IDLE, SYNC_PROTOCOL, SYNC_LAYERS, SYNC_KEYS, SYNC_WRITE } sync_stage_t;
static sync_stage_t s_sync_stage;

static uint32_t clock_ms(void *context) {
    (void)context;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void usb_state(void *context, bool connected, uint16_t vid, uint16_t pid) {
    (void)context;
    char name[32];
    snprintf(name, sizeof(name), "%04X:%04X", vid, pid);
    s_keyboard_connected = connected; s_connected_vid = vid; s_connected_pid = pid;
    if (connected && s_definition_valid) s_sync_requested = true;
    if (bsp_display_lock(100)) {
        tabvia_ui_set_connection(s_ui, connected, connected ? name : NULL);
        bsp_display_unlock();
    }
}

static void transport_done(void *context, bool success, const via_report_t *response) {
    (void)context;
    if (success && response) s_response = *response;
    else via_report_clear(&s_response);
    s_response_ready = true;
}

static void ui_message(const char *message) {
    if (bsp_display_lock(1000)) {
        tabvia_ui_set_configure_message(s_ui, message); bsp_display_unlock();
    }
}

static void publish_keymap(void) {
    if (!s_keycodes) return;
    if (bsp_display_lock(1000)) {
        tabvia_ui_set_keymap(s_ui, &s_definition, s_layers, s_keycodes);
        bsp_display_unlock();
    }
}

static void send_sync_request(void) {
    via_report_t request;
    if (s_sync_stage == SYNC_PROTOCOL) via_make_get_protocol_version(&request);
    else if (s_sync_stage == SYNC_LAYERS) via_make_get_layer_count(&request);
    else if (s_sync_stage == SYNC_KEYS)
        via_make_get_keycode(&request, s_read_layer, s_read_row, s_read_column);
    else if (s_sync_stage == SYNC_WRITE)
        via_make_set_keycode(&request, s_write_layer, s_write_row, s_write_column, s_write_keycode);
    else return;
    via_transport_request(&s_transport, &request, transport_done, NULL);
}

static void via_session_task(void *arg) {
    (void)arg;
    while (true) {
        via_transport_poll(&s_transport);
        if (s_sync_requested && !s_transport.busy) {
            s_sync_requested = false; s_sync_stage = SYNC_PROTOCOL;
            s_read_layer = s_read_row = s_read_column = 0;
            ui_message("Reading VIA protocol…"); send_sync_request();
        }
        if (s_response_ready) {
            s_response_ready = false;
            if (!s_response.bytes[0]) { s_sync_stage = SYNC_IDLE; ui_message("VIA request timed out"); }
            else if (s_sync_stage == SYNC_PROTOCOL) {
                uint16_t version = 0;
                if (!via_response_protocol_version(&s_response, &version)) { s_sync_stage = SYNC_IDLE; ui_message("Invalid VIA protocol response"); }
                else { s_sync_stage = SYNC_LAYERS; ui_message("Reading layer count…"); send_sync_request(); }
            } else if (s_sync_stage == SYNC_LAYERS) {
                s_layers = s_response.bytes[1];
                if (!s_layers || s_layers > 32) { s_sync_stage = SYNC_IDLE; ui_message("Keyboard returned an invalid layer count"); }
                else {
                    size_t count = (size_t)s_layers * s_definition.matrix_rows * s_definition.matrix_columns;
                    free(s_keycodes); s_keycodes = heap_caps_calloc(count, sizeof(*s_keycodes), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (!s_keycodes) { s_sync_stage = SYNC_IDLE; ui_message("Not enough memory for keymap"); }
                    else { s_sync_stage = SYNC_KEYS; ui_message("Reading keymap…"); send_sync_request(); }
                }
            } else if (s_sync_stage == SYNC_KEYS) {
                uint16_t keycode = 0;
                if (!via_response_keycode(&s_response, &keycode)) { s_sync_stage = SYNC_IDLE; ui_message("Invalid keymap response"); }
                else {
                    size_t index = ((size_t)s_read_layer * s_definition.matrix_rows + s_read_row) * s_definition.matrix_columns + s_read_column;
                    s_keycodes[index] = keycode;
                    if (++s_read_column >= s_definition.matrix_columns) { s_read_column = 0; if (++s_read_row >= s_definition.matrix_rows) { s_read_row = 0; s_read_layer++; } }
                    if (s_read_layer >= s_layers) { s_sync_stage = SYNC_IDLE; ui_message("Keymap ready"); publish_keymap(); }
                    else send_sync_request();
                }
            } else if (s_sync_stage == SYNC_WRITE) {
                size_t index = ((size_t)s_write_layer * s_definition.matrix_rows + s_write_row) * s_definition.matrix_columns + s_write_column;
                if (s_keycodes) s_keycodes[index] = s_write_keycode;
                s_sync_stage = SYNC_IDLE; ui_message("Keycode saved"); publish_keymap();
            }
        }
        if (s_write_requested && s_sync_stage == SYNC_IDLE && !s_transport.busy) {
            s_write_requested = false; s_sync_stage = SYNC_WRITE; ui_message("Saving keycode…"); send_sync_request();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void console_changed(void *context) {
    (void)context;
    char *text = heap_caps_malloc(16384, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!text) return;
    hid_console_read(&s_console, text, 16384);
    if (bsp_display_lock(100)) {
        tabvia_ui_set_console_text(s_ui, text, s_console.dropped);
        bsp_display_unlock();
    }
    free(text);
}

static void definition_selected(void *context, const char *path,
                                definition_version_t version) {
    (void)context;
    definition_summary_t *summary = heap_caps_calloc(1, sizeof(*summary),
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    definition_error_t *errors = heap_caps_calloc(12, sizeof(*errors),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!summary || !errors) { free(summary); free(errors); ui_message("Not enough memory to read definition"); return; }
    size_t count = 0;
    bool valid = definition_reader_validate_file(path, version, summary,
                                                  errors, 12, &count);
    if (!valid && count == 0) {
        snprintf(errors[0].instance_path, sizeof(errors[0].instance_path), "Object");
        snprintf(errors[0].message, sizeof(errors[0].message), "unable to read definition file");
        count = 1;
    }
    if (bsp_display_lock(1000)) {
        tabvia_ui_set_definition_result(s_ui, valid ? summary : NULL, errors, count);
        bsp_display_unlock();
    }
    if (valid) {
        s_definition = *summary; s_definition_valid = true;
        if (s_keyboard_connected && (summary->vendor_id != s_connected_vid || summary->product_id != s_connected_pid))
            ui_message("Definition VID/PID does not match the connected keyboard");
        else if (s_keyboard_connected) s_sync_requested = true;
        else ui_message("Definition selected; connect its VIA keyboard");
    }
    free(summary); free(errors);
}

static void key_selected(void *context, uint8_t layer, uint8_t row, uint8_t column) {
    (void)context; (void)layer; (void)row; (void)column;
}
static void keycode_apply(void *context, uint8_t layer, uint8_t row,
                          uint8_t column, uint16_t keycode) {
    (void)context;
    if (!s_keyboard_connected || !s_keycodes || layer >= s_layers ||
        row >= s_definition.matrix_rows || column >= s_definition.matrix_columns) return;
    s_write_layer = layer; s_write_row = row; s_write_column = column;
    s_write_keycode = keycode; s_write_requested = true;
}

static void console_clear(void *context) {
    (void)context;
    hid_console_clear(&s_console);
    console_changed(NULL);
}

static void console_save(void *context) {
    (void)context;
    int64_t seconds = esp_timer_get_time() / 1000000;
    char path[96];
    snprintf(path, sizeof(path), "/sdcard/tabvia/hid-console-%lld.txt", (long long)seconds);
    int result = hid_console_export(&s_console, path, "Selected keyboard", 0, 0);
    ESP_LOGI(TAG, "Console export %s: %s", path, result == 0 ? "ok" : "failed");
}

static void populate_definitions(void) {
    definition_file_t *files = heap_caps_calloc(64, sizeof(*files),
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!files) {
        ESP_LOGE(TAG, "Unable to allocate definition catalog");
        return;
    }
    size_t count = 0;
    if (definition_catalog_list("/sdcard/tabvia/definitions", files, 64, &count) == 0 &&
        bsp_display_lock(1000)) {
        tabvia_ui_set_definition_files(s_ui, files, count);
        bsp_display_unlock();
    }
    free(files);
}

void app_main(void) {
    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_io_expander_pi4ioe_init(bsp_i2c_get_handle());
    bsp_reset_tp();

    lv_display_t *display = bsp_display_start();
    if (!display) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    /* TabVIA is designed for the Tab5 in 1280x720 landscape orientation.
     * The BSP rotation helper also keeps LVGL touch coordinates aligned. */
    bsp_display_rotate(display, LV_DISPLAY_ROTATION_90);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Panel: %s", bsp_display_get_panel_ic());

    tabvia_ui_callbacks_t callbacks = {
        .definition_selected = definition_selected,
        .console_clear = console_clear,
        .console_save = console_save,
        .key_selected = key_selected,
        .keycode_apply = keycode_apply,
    };
    if (bsp_display_lock(1000)) {
        s_ui = tabvia_ui_create(lv_screen_active(), &callbacks);
        bsp_display_unlock();
    }
    if (!s_ui) {
        ESP_LOGE(TAG, "UI creation failed");
        return;
    }

    /* Let the LVGL task flush the first frame before slower peripheral setup. */
    vTaskDelay(pdMS_TO_TICKS(500));

    if (bsp_sdcard_init("/sdcard", 8) == ESP_OK) populate_definitions();
    else ESP_LOGW(TAG, "microSD unavailable; web/cache definitions can still be added later");

    s_console_storage = heap_caps_malloc(65536, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_console_storage) {
        ESP_LOGE(TAG, "Unable to allocate console buffer");
        return;
    }
    hid_console_init(&s_console, s_console_storage, 65536);
    via_transport_init(&s_transport, tabvia_usb_send_via, clock_ms, NULL, 0, 1000);
    xTaskCreate(via_session_task, "via_session", 6144, NULL, 4, NULL);

    esp_err_t usb_host_result = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (usb_host_result != ESP_OK) {
        ESP_LOGE(TAG, "USB host start failed: %s", esp_err_to_name(usb_host_result));
        return;
    }
    tabvia_usb_config_t usb = {
        .state_changed = usb_state,
        .console_changed = console_changed,
        .console = &s_console,
        .transport = &s_transport,
    };
    if (tabvia_usb_install(&usb) != 0) ESP_LOGE(TAG, "Unable to install HID host");
}
