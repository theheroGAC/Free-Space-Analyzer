#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTITIONS 4

typedef struct {
    const char* label;   
    const char* path;    
    uint8_t     present; 
    uint64_t    total_bytes;
    uint64_t    free_bytes;
} PartitionInfo;

typedef struct {
    char     name[256];
    uint64_t size_bytes;
} FolderUsage;

int fs_detect_partitions(PartitionInfo out_list[], int *out_count);

int fs_top_entries_in_root(const char* root_path, FolderUsage* out, int max_items);

void format_bytes(uint64_t bytes, char* out, int outsz);

uint64_t fs_size_by_extension(const char* root_path, const char** extensions, int ext_count);

int fs_is_directory(const char* path);

int fs_scan_directory(const char* full_path, FolderUsage* out, int max_items);

void fs_build_path(const char* current_path, const char* entry_name, char* out_path, int max_len);

int fs_delete_entry(const char* path);

#ifdef __cplusplus
}
#endif
