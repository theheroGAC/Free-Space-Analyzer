#ifndef UI_H
#define UI_H

#include "fs_analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init();
void ui_deinit();
void ui_show_splash(int duration_ms);

// Main draw (kept identical to your original signature)
void ui_draw(const PartitionInfo* parts, int parts_count, int current_index,
             const FolderUsage* top, int top_count,
             int battery_percent, int fps,
             float calc_alpha);

// NEW (optional): set a short label for the active filter shown in the top bar
void ui_set_filter_label(const char* label);

#ifdef __cplusplus
}
#endif

#endif // UI_H
