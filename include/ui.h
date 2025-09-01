#pragma once
#include <stdint.h>
#pragma once
#include "fs_analyzer.h" // PartitionInfo and FolderUsage are defined here
#include <vita2d.h>

// -----------------------------------------------------------------------------
// Initialize the UI system (load fonts, textures, etc.)
void ui_init(void);

// -----------------------------------------------------------------------------
// Free UI resources (fonts, textures, vita2d)
void ui_deinit(void);

// -----------------------------------------------------------------------------
// Show splash screen with fade-in and fade-out
// duration_ms: total duration of splash animation in milliseconds
void ui_show_splash(int duration_ms);

// -----------------------------------------------------------------------------
// Set the active filter label (used for color coding in the folder bars)
// label: string representing the active filter (e.g., "Games", "MP3")
void ui_set_filter_label(const char* label);

// -----------------------------------------------------------------------------
// Draw the full UI frame
//
// parts: array of partitions
// parts_count: number of partitions in the array
// current_part_index: index of the currently selected partition
// folders: array of folders in the current partition
// folders_count: number of folders
// battery_percent: battery level (0-100)
// fps: current FPS (currently fixed to 59)
// calc_alpha: alpha value for "Calculating..." overlay (0.0 = hidden, >0 = visible)
// current_folder_index: index of the currently selected folder
// overlay_active: 1 if the overlay filter panel is active (animated in/out from right)
// overlay_sel: index of the selected overlay entry
// overlay_labels: array of strings representing overlay entries
// overlay_count: number of entries in the overlay
void ui_draw(const PartitionInfo* parts, int parts_count, int current_part_index,
             const FolderUsage* folders, int folders_count,
             int battery_percent, int fps, float calc_alpha, int current_folder_index,
             int overlay_active, int overlay_sel,
             const char* overlay_labels[], int overlay_count);
