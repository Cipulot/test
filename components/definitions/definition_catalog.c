#include "definition_catalog.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
static int is_json(const char *name) {
    size_t n = strlen(name); return n > 5 && strcasecmp(name + n - 5, ".json") == 0;
}
int definition_catalog_list(const char *directory, definition_file_t *files,
                            size_t capacity, size_t *count) {
    if (!directory || !files || !count) return -1;
    *count = 0; DIR *dir = opendir(directory); if (!dir) return -2;
    struct dirent *entry;
    while ((entry = readdir(dir)) && *count < capacity) {
        if (entry->d_name[0] == '.' || !is_json(entry->d_name)) continue;
        definition_file_t *file = &files[*count];
        int n = snprintf(file->path, sizeof(file->path), "%s/%s", directory, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(file->path)) continue;
        struct stat st; if (stat(file->path, &st) || !S_ISREG(st.st_mode)) continue;
        size_t name_length = strnlen(entry->d_name, sizeof(file->name));
        if (name_length >= sizeof(file->name)) continue;
        memcpy(file->name, entry->d_name, name_length + 1);
        file->size_bytes = (size_t)st.st_size; (*count)++;
    }
    closedir(dir); return 0;
}
