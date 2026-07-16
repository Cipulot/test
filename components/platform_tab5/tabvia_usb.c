#include "tabvia_usb.h"
#include "hid_usage.h"
#include "esp_log.h"
#include "usb/hid_host.h"
#include <string.h>
static const char *TAG = "tabvia_usb";
typedef struct {
    hid_host_device_handle_t handle; hid_usage_profile_t profile;
    uint16_t vid, pid; bool opened;
} slot_t;
static struct { tabvia_usb_config_t config; slot_t via, console; bool installed; } s;
static void close_slot(slot_t *slot) {
    if (!slot->opened) return;
    hid_host_device_stop(slot->handle); hid_host_device_close(slot->handle);
    *slot = (slot_t){0};
}
static void interface_event(hid_host_device_handle_t handle,
                            const hid_host_interface_event_t event, void *arg) {
    slot_t *slot = arg;
    if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
        uint8_t data[65]; size_t length = 0;
        if (hid_host_device_get_raw_input_report_data(handle, data, sizeof(data), &length) != ESP_OK) return;
        uint8_t report_id = slot->profile.report_id; const uint8_t *payload = data;
        if (report_id && length && data[0] == report_id) { payload++; length--; }
        if (slot->profile.kind == HID_USAGE_VIA && s.config.transport)
            via_transport_on_input(s.config.transport, report_id, payload, length);
        else if (slot->profile.kind == HID_USAGE_QMK_CONSOLE && s.config.console) {
            hid_console_append_report(s.config.console, payload, length);
            if (s.config.console_changed) s.config.console_changed(s.config.context);
        }
    } else if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED) {
        bool was_via = slot->profile.kind == HID_USAGE_VIA;
        close_slot(slot);
        if (was_via && s.config.transport) via_transport_cancel(s.config.transport);
        if (was_via && s.config.state_changed) s.config.state_changed(s.config.context, false, 0, 0);
    } else if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
        ESP_LOGW(TAG, "HID transfer error");
    }
}
static void connected(hid_host_device_handle_t handle,
                      const hid_host_driver_event_t event, void *arg) {
    (void)arg; if (event != HID_HOST_DRIVER_EVENT_CONNECTED) return;
    size_t descriptor_length = 0;
    uint8_t *descriptor = hid_host_get_report_descriptor(handle, &descriptor_length);
    hid_usage_profile_t profiles[2];
    size_t count = hid_usage_scan_report_descriptor(descriptor, descriptor_length, profiles, 2);
    if (!count) return;
    hid_host_dev_info_t info = {0};
    if (hid_host_get_device_info(handle, &info) != ESP_OK) return;
    for (size_t i = 0; i < count && i < 2; i++) {
        slot_t *slot = profiles[i].kind == HID_USAGE_VIA ? &s.via : &s.console;
        if (slot->opened) continue;
        *slot = (slot_t){.handle = handle, .profile = profiles[i], .vid = info.VID, .pid = info.PID};
        hid_host_device_config_t cfg = {.callback = interface_event, .callback_arg = slot};
        if (hid_host_device_open(handle, &cfg) == ESP_OK && hid_host_device_start(handle) == ESP_OK) {
            slot->opened = true;
            if (slot == &s.via && s.config.state_changed)
                s.config.state_changed(s.config.context, true, info.VID, info.PID);
        } else { ESP_LOGE(TAG, "Unable to open HID interface"); *slot = (slot_t){0}; }
        /* One handle represents one HID interface in usb_host_hid. */
        break;
    }
}
int tabvia_usb_install(const tabvia_usb_config_t *config) {
    if (!config || s.installed) return -1;
    memset(&s, 0, sizeof(s));
    s.config = *config;
    hid_host_driver_config_t cfg = {.create_background_task = true, .task_priority = 5,
        .stack_size = 4096, .core_id = tskNO_AFFINITY, .callback = connected};
    if (hid_host_install(&cfg) != ESP_OK) return -2;
    s.installed = true;
    return 0;
}
int tabvia_usb_uninstall(void) {
    if (!s.installed) return 0;
    close_slot(&s.via);
    close_slot(&s.console);
    if (hid_host_uninstall() != ESP_OK) return -1;
    memset(&s, 0, sizeof(s));
    return 0;
}
bool tabvia_usb_send_via(void *context, uint8_t report_id,
                         const uint8_t *data, size_t length) {
    (void)context; if (!s.via.opened || !data || length != VIA_REPORT_SIZE) return false;
    return hid_class_request_set_report(s.via.handle, HID_REPORT_TYPE_OUTPUT,
        report_id, (uint8_t *)data, length) == ESP_OK;
}
