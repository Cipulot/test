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

static uint32_t clock_ms(void *context) {
    (void)context;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void usb_state(void *context, bool connected, uint16_t vid, uint16_t pid) {
    (void)context;
    char name[32];
    snprintf(name, sizeof(name), "%04X:%04X", vid, pid);
    if (bsp_display_lock(100)) {
        tabvia_ui_set_connection(s_ui, connected, connected ? name : NULL);
        bsp_display_unlock();
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
    definition_summary_t summary;
    definition_error_t errors[12];
    size_t count = 0;
    bool valid = definition_reader_validate_file(path, version, &summary,
                                                  errors, 12, &count);
    if (!valid && count == 0) {
        snprintf(errors[0].instance_path, sizeof(errors[0].instance_path), "Object");
        snprintf(errors[0].message, sizeof(errors[0].message), "unable to read definition file");
        count = 1;
    }
    if (bsp_display_lock(1000)) {
        tabvia_ui_set_definition_result(s_ui, valid ? &summary : NULL, errors, count);
        bsp_display_unlock();
    }
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
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Panel: %s", bsp_display_get_panel_ic());

    tabvia_ui_callbacks_t callbacks = {
        .definition_selected = definition_selected,
        .console_clear = console_clear,
        .console_save = console_save,
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
