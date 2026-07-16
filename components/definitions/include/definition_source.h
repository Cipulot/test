#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    DEFINITION_SOURCE_SD_EXACT,
    DEFINITION_SOURCE_SD_MANUAL,
    DEFINITION_SOURCE_CACHE,
    DEFINITION_SOURCE_WEB
} definition_source_kind_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    char product_name[64];
} definition_identity_t;

int definition_exact_sd_path(char *out, size_t out_size,
                             const char *mount, uint16_t vendor_id,
                             uint16_t product_id);
bool definition_identity_matches(const definition_identity_t *definition,
                                 uint16_t vendor_id, uint16_t product_id);
