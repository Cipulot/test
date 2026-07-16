#pragma once
#include "via_protocol.h"
#include <stdbool.h>
#include <stdint.h>
typedef bool (*via_transport_send_fn)(void *context, uint8_t report_id,
                                      const uint8_t *data, size_t length);
typedef uint32_t (*via_transport_clock_fn)(void *context);
typedef void (*via_transport_done_fn)(void *context, bool success,
                                      const via_report_t *response);
typedef struct {
    via_transport_send_fn send; via_transport_clock_fn clock;
    void *io_context; uint8_t report_id; uint32_t timeout_ms;
    bool busy; uint32_t started_at; via_report_t request;
    via_transport_done_fn done; void *done_context;
} via_transport_t;
void via_transport_init(via_transport_t *transport, via_transport_send_fn send,
                        via_transport_clock_fn clock, void *io_context,
                        uint8_t report_id, uint32_t timeout_ms);
bool via_transport_request(via_transport_t *transport, const via_report_t *request,
                           via_transport_done_fn done, void *done_context);
bool via_transport_on_input(via_transport_t *transport, uint8_t report_id,
                            const uint8_t *data, size_t length);
void via_transport_poll(via_transport_t *transport);
void via_transport_cancel(via_transport_t *transport);
