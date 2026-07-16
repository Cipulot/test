#include "definition_source.h"
#include <stdio.h>

int definition_exact_sd_path(char *out, size_t out_size, const char *mount,
                             uint16_t vendor_id, uint16_t product_id) {
    if (!out || !out_size || !mount) return -1;
    int n = snprintf(out, out_size, "%s/tabvia/definitions/%04x-%04x.json",
                     mount, vendor_id, product_id);
    return (n < 0 || (size_t)n >= out_size) ? -1 : n;
}

bool definition_identity_matches(const definition_identity_t *definition,
                                 uint16_t vendor_id, uint16_t product_id) {
    return definition && definition->vendor_id == vendor_id &&
           definition->product_id == product_id;
}
