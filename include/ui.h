#ifndef UI_H
#define UI_H

#include "fs_analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init();
void ui_deinit();
void ui_show_splash(int duration_ms);

// Aggiornata con current_folder_index per D-Pad scroll
void ui_draw(const PartitionInfo* parts, int parts_count, int current_part_index,
             const FolderUsage* folders, int folders_count,
             int battery_percent, int fps,
             float calc_alpha,
             int current_folder_index);

// Setta il filtro attivo visualizzato nella barra
void ui_set_filter_label(const char* label);

#ifdef __cplusplus
}
#endif

#endif // UI_H
