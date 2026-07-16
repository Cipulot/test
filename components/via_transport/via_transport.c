#include "via_transport.h"
#include <string.h>
void via_transport_init(via_transport_t *t, via_transport_send_fn send,
                        via_transport_clock_fn clock, void *ctx,
                        uint8_t report_id, uint32_t timeout_ms) {
    memset(t, 0, sizeof(*t)); t->send = send; t->clock = clock;
    t->io_context = ctx; t->report_id = report_id; t->timeout_ms = timeout_ms;
}
bool via_transport_request(via_transport_t *t, const via_report_t *r,
                           via_transport_done_fn done, void *done_ctx) {
    if (!t || !r || !t->send || !t->clock || t->busy) return false;
    t->request = *r; t->done = done; t->done_context = done_ctx;
    t->started_at = t->clock(t->io_context); t->busy = true;
    if (!t->send(t->io_context, t->report_id, r->bytes, VIA_REPORT_SIZE)) {
        t->busy = false; if (done) done(done_ctx, false, NULL); return false;
    } return true;
}
bool via_transport_on_input(via_transport_t *t, uint8_t report_id,
                            const uint8_t *data, size_t length) {
    if (!t || !t->busy || !data || length < VIA_REPORT_SIZE ||
        report_id != t->report_id || data[0] != t->request.bytes[0]) return false;
    via_report_t response; memcpy(response.bytes, data, VIA_REPORT_SIZE);
    via_transport_done_fn done = t->done; void *ctx = t->done_context;
    t->busy = false; t->done = NULL; if (done) done(ctx, true, &response); return true;
}
void via_transport_poll(via_transport_t *t) {
    if (t && t->busy && (uint32_t)(t->clock(t->io_context) - t->started_at) >= t->timeout_ms) {
        via_transport_done_fn done = t->done; void *ctx = t->done_context;
        t->busy = false; t->done = NULL; if (done) done(ctx, false, NULL);
    }
}
void via_transport_cancel(via_transport_t *t) {
    if (!t || !t->busy) return;
    via_transport_done_fn done = t->done;
    void *ctx = t->done_context;
    t->busy = false; t->done = NULL; if (done) done(ctx, false, NULL);
}
