#ifndef UI_H
#define UI_H

#include "fs_analyzer.h"

void ui_init();
void ui_deinit();
void ui_draw(const PartitionInfo* parts, int parts_count, int current_index,
             const FolderUsage* top, int top_count,
             int battery_percent, int fps,
             float calc_alpha);

// Aggiunta dichiarazione dello splash screen
void ui_show_splash(int duration_ms);

#endif // UI_H
