#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTITIONS 4

typedef struct {
    const char* label;   // "ux0", "ur0", ...
    const char* path;    // "ux0:/"
    uint8_t     present; // 1 if mounted/accessible
    uint64_t    total_bytes;
    uint64_t    free_bytes;
} PartitionInfo;

typedef struct {
    char     name[256];
    uint64_t size_bytes; // recursive size
} FolderUsage;

// Detect mounted partitions (ux0, ur0, uma0, imc0)
int fs_detect_partitions(PartitionInfo out_list[], int *out_count);

// Scan root path and return top N biggest entries (folders/files)
int fs_top_entries_in_root(const char* root_path, FolderUsage* out, int max_items);

// Human-readable formatting (B/KB/MB/GB/TB)
void format_bytes(uint64_t bytes, char* out, int outsz);

// NEW: calculate total size for a set of extensions (case-insensitive)
// extensions: array of strings like ".mp3" or "mp3"; ext_count = number of valid items in the array
uint64_t fs_size_by_extension(const char* root_path, const char** extensions, int ext_count);

// NAVIGATION FUNCTIONS
// Check if a path is a directory
int fs_is_directory(const char* path);

// Navigate into a subdirectory and get its contents
int fs_scan_directory(const char* full_path, FolderUsage* out, int max_items);

// Build full path from current path + entry name
void fs_build_path(const char* current_path, const char* entry_name, char* out_path, int max_len);

#ifdef __cplusplus
}
#endif
