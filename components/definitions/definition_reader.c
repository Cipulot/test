#include "definition_reader.h"
#include "cJSON.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { definition_error_t *items; size_t capacity, count; } errors_t;
static void error_add(errors_t *e, const char *path, const char *message) {
    if (e->count < e->capacity) {
        snprintf(e->items[e->count].instance_path, sizeof(e->items[e->count].instance_path), "%s", path);
        snprintf(e->items[e->count].message, sizeof(e->items[e->count].message), "%s", message);
    }
    e->count++;
}
static cJSON *required(errors_t *e, const cJSON *obj, const char *name, const char *path) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, name);
    if (!item) {
        error_add(e, path, "must have required property");
    }
    return item;
}
static bool parse_hex_id(const cJSON *item, uint16_t *out) {
    if (!cJSON_IsString(item) || !item->valuestring) return false;
    const char *s = item->valuestring;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) {
        return false;
    }
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(s, &end, 16);
    if (errno || *end || value > 0xfffful) return false;
    *out = (uint16_t)value;
    return true;
}
static void validate_matrix(const cJSON *root, definition_summary_t *out, errors_t *e) {
    cJSON *matrix = required(e, root, "matrix", "/matrix");
    if (!cJSON_IsObject(matrix)) { if (matrix) error_add(e, "/matrix", "must be object"); return; }
    cJSON *rows = required(e, matrix, "rows", "/matrix/rows");
    cJSON *cols = required(e, matrix, "cols", "/matrix/cols");
    if (!cJSON_IsNumber(rows) || rows->valueint < 1 || rows->valueint > 255 || rows->valuedouble != rows->valueint)
        error_add(e, "/matrix/rows", "must be integer between 1 and 255");
    else out->matrix_rows = (uint16_t)rows->valueint;
    if (!cJSON_IsNumber(cols) || cols->valueint < 1 || cols->valueint > 255 || cols->valuedouble != cols->valueint)
        error_add(e, "/matrix/cols", "must be integer between 1 and 255");
    else out->matrix_columns = (uint16_t)cols->valueint;
}
static void validate_layouts(const cJSON *root, definition_shape_t shape,
                             definition_summary_t *out, errors_t *e) {
    cJSON *layouts = required(e, root, "layouts", "/layouts");
    if (!cJSON_IsObject(layouts)) { if (layouts) error_add(e, "/layouts", "must be object"); return; }
    const char *member = shape == DEFINITION_SHAPE_KEYBOARD ? "keymap" : "keys";
    char path[32]; snprintf(path, sizeof(path), "/layouts/%s", member);
    cJSON *keys = required(e, layouts, member, path);
    if (!cJSON_IsArray(keys) || cJSON_GetArraySize(keys) == 0) {
        if (keys) {
            error_add(e, path, "must be a non-empty array");
        }
        return;
    }
    out->layout_key_count = (size_t)cJSON_GetArraySize(keys);
}
bool definition_reader_validate(const char *json, definition_version_t version,
    definition_summary_t *out, definition_error_t *items, size_t capacity,
    size_t *error_count) {
    if (!json || !out || !error_count || (capacity && !items)) return false;
    memset(out, 0, sizeof(*out)); errors_t e = {items, capacity, 0};
    cJSON *root = cJSON_Parse(json);
    if (!root) { error_add(&e, "", "must be valid JSON"); *error_count = e.count; return false; }
    if (!cJSON_IsObject(root)) { error_add(&e, "", "must be object"); goto done; }
    out->version = version;
    cJSON *name = required(&e, root, "name", "/name");
    if (!cJSON_IsString(name) || !name->valuestring || !name->valuestring[0])
        error_add(&e, "/name", "must be non-empty string");
    else snprintf(out->name, sizeof(out->name), "%s", name->valuestring);
    cJSON *vp = cJSON_GetObjectItemCaseSensitive(root, "vendorProductId");
    cJSON *vendor = cJSON_GetObjectItemCaseSensitive(root, "vendorId");
    cJSON *product = cJSON_GetObjectItemCaseSensitive(root, "productId");
    if (vp) {
        out->shape = DEFINITION_SHAPE_VIA;
        if (!cJSON_IsNumber(vp) || vp->valuedouble < 0 || vp->valuedouble > 4294967295.0 || vp->valuedouble != (double)(uint32_t)vp->valuedouble)
            error_add(&e, "/vendorProductId", "must be uint32");
        else { out->vendor_product_id = (uint32_t)vp->valuedouble;
            out->vendor_id = (uint16_t)(out->vendor_product_id >> 16); out->product_id = (uint16_t)out->vendor_product_id; }
    } else {
        out->shape = DEFINITION_SHAPE_KEYBOARD;
        if (!vendor) error_add(&e, "/vendorId", "must have required property");
        else if (!parse_hex_id(vendor, &out->vendor_id)) error_add(&e, "/vendorId", "must be a hexadecimal uint16 string");
        if (!product) error_add(&e, "/productId", "must have required property");
        else if (!parse_hex_id(product, &out->product_id)) error_add(&e, "/productId", "must be a hexadecimal uint16 string");
        out->vendor_product_id = ((uint32_t)out->vendor_id << 16) | out->product_id;
    }
    if (version == DEFINITION_V2 && out->shape == DEFINITION_SHAPE_KEYBOARD && !cJSON_HasObjectItem(root, "lighting"))
        error_add(&e, "/lighting", "must have required property");
    if (version == DEFINITION_V3 && cJSON_HasObjectItem(root, "lighting"))
        error_add(&e, "/lighting", "must NOT have additional properties");
    validate_matrix(root, out, &e); validate_layouts(root, out->shape, out, &e);
    out->has_menus = cJSON_HasObjectItem(root, "menus") || cJSON_HasObjectItem(root, "customMenus");
    out->has_custom_keycodes = cJSON_HasObjectItem(root, "customKeycodes");
done: cJSON_Delete(root); *error_count = e.count; return e.count == 0;
}
bool definition_reader_validate_file(const char *path, definition_version_t version,
    definition_summary_t *out, definition_error_t *errors, size_t capacity,
    size_t *error_count) {
    if (!path) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    if (fseek(file, 0, SEEK_END)) { fclose(file); return false; }
    long length = ftell(file);
    if (length < 0 || length > 1024 * 1024 || fseek(file, 0, SEEK_SET)) { fclose(file); return false; }
    char *json = malloc((size_t)length + 1); if (!json) { fclose(file); return false; }
    size_t read = fread(json, 1, (size_t)length, file); fclose(file); json[read] = 0;
    bool ok = read == (size_t)length && definition_reader_validate(json, version, out, errors, capacity, error_count);
    free(json); return ok;
}
