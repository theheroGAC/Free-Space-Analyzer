#pragma once
#include <stdint.h>
#include "fs_analyzer.h"
#include <vita2d.h>

void ui_init(void);

void ui_deinit(void);

void ui_set_filter_label(const char* label);

void ui_draw(const PartitionInfo* parts, int parts_count, int current_part_index,
             const FolderUsage* folders, int folders_count,
             int battery_percent, int fps, float calc_alpha, int current_folder_index,
             int overlay_active, int overlay_sel,
             const char* overlay_labels[], int overlay_count,
             int delete_confirm_active, const char* delete_confirm_name,
             int startup_active);
