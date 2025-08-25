#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTITIONS 4

typedef struct {
    const char* label;   // "ux0", "ur0", ...
    const char* path;    // "ux0:/"
    uint8_t     present; // 1 se montata/accessibile
    uint64_t    total_bytes;
    uint64_t    free_bytes;
} PartitionInfo;

typedef struct {
    char   name[256];
    uint64_t size_bytes; // dimensione ricorsiva
} FolderUsage;

int fs_detect_partitions(PartitionInfo out_list[], int *out_count);
int fs_top_entries_in_root(const char* root_path, FolderUsage* out, int max_items);
void format_bytes(uint64_t bytes, char* out, int outsz);

#ifdef __cplusplus
}
#endif