#pragma once
#include "hid_console.h"
#include "via_transport.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef void (*tabvia_usb_state_cb)(void *context, bool connected,
                                    uint16_t vendor_id, uint16_t product_id);
typedef void (*tabvia_usb_console_cb)(void *context);
typedef struct {
    tabvia_usb_state_cb state_changed;
    tabvia_usb_console_cb console_changed;
    void *context;
    hid_console_t *console;
    via_transport_t *transport;
} tabvia_usb_config_t;
/* The application must install usb_host first and keep its daemon task running. */
int tabvia_usb_install(const tabvia_usb_config_t *config);
int tabvia_usb_uninstall(void);
/* Adapter used as via_transport_send_fn. context is ignored. */
bool tabvia_usb_send_via(void *context, uint8_t report_id,
                         const uint8_t *data, size_t length);
