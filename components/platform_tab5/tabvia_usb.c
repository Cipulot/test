#include "tabvia_usb.h"
#include "hid_usage.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
static const char *TAG = "tabvia_usb";
typedef struct {
    hid_host_device_handle_t handle; hid_usage_profile_t profile;
    uint16_t vid, pid; bool opened, started, disconnected;
    uint8_t out_endpoint; uint16_t out_mps; usb_transfer_t *out_transfer;
    volatile bool out_busy;
} slot_t;

/* usb_host_hid 1.2 does not expose its interrupt OUT endpoint or an output
 * submission API. These two prefix mirrors are pinned to that component's
 * public 1.2 layout and are used only to reach the USB device handle. */
typedef struct tabvia_hid_device_prefix {
    STAILQ_ENTRY(tabvia_hid_device_prefix) tailq_entry;
    SemaphoreHandle_t device_busy;
    SemaphoreHandle_t ctrl_xfer_done;
    usb_transfer_t *ctrl_xfer;
    usb_device_handle_t dev_hdl;
} tabvia_hid_device_prefix_t;
typedef struct tabvia_hid_interface_prefix {
    STAILQ_ENTRY(tabvia_hid_interface_prefix) tailq_entry;
    tabvia_hid_device_prefix_t *parent;
    hid_host_dev_params_t dev_params;
} tabvia_hid_interface_prefix_t;
static struct {
    tabvia_usb_config_t config; slot_t *via, *console; bool installed;
    QueueHandle_t discovery_queue; TaskHandle_t discovery_task;
} s;
static void out_transfer_done(usb_transfer_t *transfer) {
    slot_t *slot = transfer ? transfer->context : NULL;
    if (!slot) return;
    slot->out_busy = false;
    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED)
        ESP_LOGW(TAG, "VIA interrupt OUT failed, status %d", transfer->status);
}
static bool prepare_out_endpoint(slot_t *slot) {
    tabvia_hid_interface_prefix_t *iface = (tabvia_hid_interface_prefix_t *)slot->handle;
    if (!iface || !iface->parent) return false;
    const usb_config_desc_t *config = NULL;
    if (usb_host_get_active_config_descriptor(iface->parent->dev_hdl, &config) != ESP_OK || !config) return false;
    const uint8_t *raw = (const uint8_t *)config; size_t offset = 0; bool matching = false;
    while (offset + 2 <= config->wTotalLength && raw[offset]) {
        uint8_t length = raw[offset], type = raw[offset + 1];
        if (offset + length > config->wTotalLength) break;
        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE && length >= 9)
            matching = raw[offset + 2] == iface->dev_params.iface_num;
        else if (matching && type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && length >= 7) {
            uint8_t address = raw[offset + 2], attributes = raw[offset + 3] & 0x03;
            if (!(address & 0x80u) && attributes == 0x03u) {
                slot->out_endpoint = address;
                slot->out_mps = (uint16_t)(raw[offset + 4] | ((uint16_t)raw[offset + 5] << 8));
                if (usb_host_transfer_alloc(slot->out_mps, 0, &slot->out_transfer) != ESP_OK) return false;
                slot->out_transfer->device_handle = iface->parent->dev_hdl;
                slot->out_transfer->bEndpointAddress = address;
                slot->out_transfer->callback = out_transfer_done;
                slot->out_transfer->context = slot;
                slot->out_transfer->timeout_ms = 1000;
                ESP_LOGI(TAG, "VIA interrupt OUT endpoint 0x%02X, MPS %u", address, slot->out_mps);
                return true;
            }
        }
        offset += length;
    }
    ESP_LOGW(TAG, "HID interface has no interrupt OUT endpoint; using SET_REPORT");
    return false;
}
static void close_slot(slot_t *slot) {
    if (!slot) return;
    if (slot->started) hid_host_device_stop(slot->handle);
    if (slot->out_transfer && !slot->out_busy) usb_host_transfer_free(slot->out_transfer);
    if (slot->opened) hid_host_device_close(slot->handle);
    if (s.via == slot) s.via = NULL;
    if (s.console == slot) s.console = NULL;
    free(slot);
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
        /* An interface can disappear while the discovery worker is waiting
         * for its report descriptor. Let that worker own pre-start cleanup. */
        if (!slot->started) {
            slot->disconnected = true;
            return;
        }
        close_slot(slot);
        if (was_via && s.config.transport) via_transport_cancel(s.config.transport);
        if (was_via && s.config.state_changed) s.config.state_changed(s.config.context, false, 0, 0);
    } else if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
        ESP_LOGW(TAG, "HID transfer error");
    }
}

static void discover_interface(void *arg) {
    (void)arg;
    slot_t *slot;
    while (xQueueReceive(s.discovery_queue, &slot, portMAX_DELAY) == pdTRUE) {
        if (!slot || slot->disconnected) {
            close_slot(slot);
            continue;
        }

        /* Run the synchronous class request outside the HID driver's callback
         * task, which must remain available to complete the USB transfer. */
        size_t descriptor_length = 0;
        uint8_t *descriptor = hid_host_get_report_descriptor(
            slot->handle, &descriptor_length);
        hid_usage_profile_t profiles[2];
        size_t count = descriptor ? hid_usage_scan_report_descriptor(
                                        descriptor, descriptor_length, profiles, 2) : 0;
        if (!count || slot->disconnected) {
            close_slot(slot);  /* Ordinary keyboard/mouse or disconnected. */
            continue;
        }

        slot->profile = profiles[0];
        slot_t **destination = slot->profile.kind == HID_USAGE_VIA ? &s.via : &s.console;
        if (*destination) {
            ESP_LOGW(TAG, "Ignoring duplicate %s HID interface",
                     slot->profile.kind == HID_USAGE_VIA ? "VIA" : "console");
            close_slot(slot);
            continue;
        }
        *destination = slot;
        if (slot->profile.kind == HID_USAGE_VIA) prepare_out_endpoint(slot);
        if (hid_host_device_start(slot->handle) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to start HID interface");
            close_slot(slot);
            continue;
        }
        slot->started = true;
        if (slot->profile.kind == HID_USAGE_VIA && s.config.state_changed)
            s.config.state_changed(s.config.context, true, slot->vid, slot->pid);
    }
    vTaskDelete(NULL);
}

static void connected(hid_host_device_handle_t handle,
                      const hid_host_driver_event_t event, void *arg) {
    (void)arg; if (event != HID_HOST_DRIVER_EVENT_CONNECTED) return;
    hid_host_dev_info_t info = {0};
    if (hid_host_get_device_info(handle, &info) != ESP_OK) return;

    /* Opening is safe in the driver callback. Descriptor retrieval is queued
     * because it is a synchronous class request serviced by the driver task. */
    slot_t *slot = calloc(1, sizeof(*slot));
    if (!slot) return;
    slot->handle = handle; slot->vid = info.VID; slot->pid = info.PID;
    hid_host_device_config_t cfg = {.callback = interface_event, .callback_arg = slot};
    if (hid_host_device_open(handle, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open HID interface");
        free(slot);
        return;
    }
    slot->opened = true;
    if (xQueueSend(s.discovery_queue, &slot, 0) != pdTRUE) {
        ESP_LOGE(TAG, "HID discovery queue full");
        close_slot(slot);
    }
}
int tabvia_usb_install(const tabvia_usb_config_t *config) {
    if (!config || s.installed) return -1;
    memset(&s, 0, sizeof(s));
    s.config = *config;
    s.discovery_queue = xQueueCreate(8, sizeof(slot_t *));
    if (!s.discovery_queue) return -2;
    if (xTaskCreate(discover_interface, "via_hid_discovery", 4096, NULL, 5,
                    &s.discovery_task) != pdPASS) {
        vQueueDelete(s.discovery_queue);
        memset(&s, 0, sizeof(s));
        return -3;
    }
    hid_host_driver_config_t cfg = {.create_background_task = true, .task_priority = 5,
        .stack_size = 4096, .core_id = tskNO_AFFINITY, .callback = connected};
    if (hid_host_install(&cfg) != ESP_OK) {
        vTaskDelete(s.discovery_task);
        vQueueDelete(s.discovery_queue);
        memset(&s, 0, sizeof(s));
        return -4;
    }
    s.installed = true;
    return 0;
}
int tabvia_usb_uninstall(void) {
    if (!s.installed) return 0;
    close_slot(s.via);
    close_slot(s.console);
    if (hid_host_uninstall() != ESP_OK) return -1;
    vTaskDelete(s.discovery_task);
    vQueueDelete(s.discovery_queue);
    memset(&s, 0, sizeof(s));
    return 0;
}
bool tabvia_usb_send_via(void *context, uint8_t report_id,
                         const uint8_t *data, size_t length) {
    (void)context; if (!s.via || !s.via->started || !data || length != VIA_REPORT_SIZE) return false;
    if (s.via->out_transfer) {
        if (s.via->out_busy) return false;
        size_t packet_length = length + (report_id ? 1u : 0u);
        if (packet_length > s.via->out_mps) return false;
        if (report_id) { s.via->out_transfer->data_buffer[0] = report_id;
            memcpy(&s.via->out_transfer->data_buffer[1], data, length); }
        else memcpy(s.via->out_transfer->data_buffer, data, length);
        s.via->out_transfer->num_bytes = packet_length; s.via->out_busy = true;
        if (usb_host_transfer_submit(s.via->out_transfer) == ESP_OK) return true;
        s.via->out_busy = false; return false;
    }
    return hid_class_request_set_report(s.via->handle, HID_REPORT_TYPE_OUTPUT,
        report_id, (uint8_t *)data, length) == ESP_OK;
}
