#include "ui.h"
#include "fs_analyzer.h"
#include <vita2d.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static vita2d_pgf* g_font = NULL;
static char g_filter_label[64] = "All";

// Scroll & display state
static int g_scroll_offset = 0;
static int g_max_visible = 10;

// Overlay animation state
static float overlay_offset_x = 960.0f; 
static int overlay_target = 0;           
static const float overlay_speed = 15.0f;

// FPS real
static uint64_t g_last_time = 0;
static float g_fps_real = 0.0f;
static const float FPS_SMOOTH_FACTOR = 0.1f;

static inline uint32_t COL(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return RGBA8(r, g, b, a); }

// Color helpers for different filter types
static uint32_t color_for_filter_label(void) {
    if (strcasecmp(g_filter_label, "Games") == 0)    return COL(230, 90, 90, 255);
    if (strcasecmp(g_filter_label, "MP3") == 0)      return COL(90, 220, 120, 255);
    if (strcasecmp(g_filter_label, "OGG") == 0)      return COL(120, 220, 255, 255);
    if (strcasecmp(g_filter_label, "Photo") == 0)    return COL(255, 170, 90, 255);
    if (strcasecmp(g_filter_label, "Video") == 0)    return COL(200, 120, 255, 255);
    if (strcasecmp(g_filter_label, "Docs") == 0)     return COL(255, 220, 120, 255);
    if (strcasecmp(g_filter_label, "Archives") == 0) return COL(180, 130, 255, 255);
    if (strcasecmp(g_filter_label, "Homebrew") == 0) return COL(255, 100, 180, 255);
    if (strcasecmp(g_filter_label, "SaveData") == 0) return COL(100, 180, 255, 255);
    return COL(90, 190, 90, 255);
}

static uint32_t color_for_usage(float fill01) {
    if (fill01 < 0.5f) return COL(90, 220, 90, 255);
    if (fill01 < 0.8f) return COL(220, 220, 90, 255);
    return COL(220, 90, 90, 255);
}

static void ui_format_bytes(uint64_t bytes, char* out, int outsz) {
    const char* units[] = {"B","KB","MB","GB","TB"};
    int unit = 0;
    double b = (double)bytes;
    while(b >= 1024.0 && unit < 4){ b /= 1024.0; unit++; }
    snprintf(out, outsz, "%.2f %s", b, units[unit]);
}

void ui_set_filter_label(const char* label) {
    if (!label || !*label) { snprintf(g_filter_label, sizeof(g_filter_label), "All"); return; }
    snprintf(g_filter_label, sizeof(g_filter_label), "%s", label);
}

void ui_init() {
    vita2d_init();
    g_font = vita2d_load_default_pgf();
    g_last_time = sceKernelGetSystemTimeWide();
}

void ui_deinit() {
    if (g_font)     { vita2d_free_pgf(g_font);         g_font = NULL;     }
    vita2d_fini();
}

static void draw_bar(float x, float y, float w, float h, float fill01, uint32_t fill_color) {
    if (fill01 < 0) fill01 = 0;
    if (fill01 > 1) fill01 = 1;

    vita2d_draw_rectangle(x + 1, y + 1, w, h, COL(20, 20, 20, 120));
    vita2d_draw_rectangle(x, y, w, h, COL(60, 60, 60, 200));
    vita2d_draw_rectangle(x + 1, y + 1, w - 2, h - 2, COL(40, 40, 40, 255));

    if (fill01 > 0) {
        for(int i = 0; i < w * fill01; i++) {
            float alpha = (float)i / (float)(w * fill01);
            uint32_t grad_color = COL(
                (uint8_t)((fill_color >> 16 & 0xFF) * (0.7f + alpha * 0.3f)),
                (uint8_t)((fill_color >> 8 & 0xFF) * (0.7f + alpha * 0.3f)),
                (uint8_t)((fill_color & 0xFF) * (0.7f + alpha * 0.3f)),
                255
            );
            vita2d_draw_rectangle(x + 1 + i, y + 1, 1, h - 2, grad_color);
        }
    }
}

void ui_update_fps() {
    uint64_t now = sceKernelGetSystemTimeWide();
    if (g_last_time != 0) {
        float dt = (now - g_last_time) / 1000000.0f;
        if (dt > 0.0f) {
            float fps_current = 1.0f / dt;
            g_fps_real = g_fps_real * (1.0f - FPS_SMOOTH_FACTOR) + fps_current * FPS_SMOOTH_FACTOR;
        }
    }
    g_last_time = now;
}


// ---- Draw full UI ----
void ui_draw(const PartitionInfo* parts, int parts_count, int current_part_index,
             const FolderUsage* folders, int folders_count,
             int battery_percent, int fps_unused, float calc_alpha, int current_folder_index,
             int overlay_active, int overlay_sel,
             const char* overlay_labels[], int overlay_count,
             int delete_confirm_active, const char* delete_confirm_name,
             int startup_active) {

    ui_update_fps();

    overlay_target = overlay_active ? 1 : 0;
    const float target_x = 960 - 220 - 20;
    if(overlay_target){
        if(overlay_offset_x > target_x) overlay_offset_x -= overlay_speed;
        if(overlay_offset_x < target_x) overlay_offset_x = target_x;
    } else {
        if(overlay_offset_x < 960) overlay_offset_x += overlay_speed;
        if(overlay_offset_x > 960) overlay_offset_x = 960;
    }

    vita2d_start_drawing();
    vita2d_clear_screen();

    // Completely solid black background - no gradients or effects
    vita2d_draw_rectangle(0, 0, 960, 544, COL(0, 0, 0, 255));

    // Draw header with nice color for "Free Space Analyzer" title
    vita2d_draw_rectangle(0, 0, 960, 50, COL(40, 60, 100, 255));
    // Add subtle border around header
    vita2d_draw_rectangle(0, 0, 960, 1, COL(80, 120, 180, 255));
    vita2d_draw_rectangle(0, 49, 960, 1, COL(80, 120, 180, 255));

    // Section separators - black to blend with background
    vita2d_draw_rectangle(0, 55, 960, 2, COL(0, 0, 0, 255));
    vita2d_draw_rectangle(0, 195, 960, 2, COL(0, 0, 0, 255));
    vita2d_draw_rectangle(0, 235, 960, 2, COL(0, 0, 0, 255));
    vita2d_draw_rectangle(0, 255, 960, 2, COL(0, 0, 0, 255));

    if(startup_active) {
        vita2d_draw_rectangle(0, 0, 960, 544, COL(0, 0, 0, 255));
        vita2d_pgf_draw_text(g_font, 480 - vita2d_pgf_text_width(g_font, 1.5f, "Checking space of various partitions...")/2.0f,
                           272 - 20, COL(255,255,255,255), 1.5f, "Checking space of various partitions...");
        vita2d_end_drawing();
        vita2d_swap_buffers();
        return;
    }

    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Free Space Analyzer | Battery: %d%% | FPS: %.1f", battery_percent, g_fps_real);
    vita2d_pgf_draw_text(g_font, 24, 36, COL(255,255,255,255), 1.0f, hdr);

    if(calc_alpha>0.0f){
        uint8_t alpha = (uint8_t)(255.0f * calc_alpha);
        float msg_width = vita2d_pgf_text_width(g_font, 1.4f, "Updating... Please wait");
        float msg_x = 480 - msg_width/2.0f;
        float msg_y = 85;

        vita2d_draw_rectangle(msg_x - 20, msg_y - 25, msg_width + 40, 35, COL(20, 40, 60, (uint8_t)(alpha * 0.8f)));
        vita2d_draw_rectangle(msg_x - 18, msg_y - 23, msg_width + 36, 31, COL(40, 80, 120, (uint8_t)(alpha * 0.6f)));
        vita2d_draw_rectangle(msg_x - 22, msg_y - 27, msg_width + 44, 39, COL(60, 120, 180, (uint8_t)(alpha * 0.3f)));

        vita2d_pgf_draw_text(g_font, msg_x, msg_y, COL(150,255,150,alpha), 1.4f, "Updating... Please wait");
    }

    // Move partitions lower and make them look nicer
    float px = 24, py = 120; 
    for(int i=0;i<parts_count;i++){
        const PartitionInfo* p = &parts[i];
        char tbuf[64], fbuf[64], line[256];
        ui_format_bytes(p->total_bytes, tbuf, sizeof(tbuf));
        ui_format_bytes(p->free_bytes, fbuf, sizeof(fbuf));
        snprintf(line,sizeof(line),"%s:/  %s free / %s total", p->label,fbuf,tbuf);
        uint32_t color = (i==current_part_index)?COL(120,255,120,255):COL(200,220,240,255);

        // Add even longer background highlight for current partition
        if(i==current_part_index) {
            vita2d_draw_rectangle(px - 24, py + i*32 - 24, 700, 28,
                COL(60, 100, 140, 150));
            vita2d_draw_rectangle(px - 26, py + i*32 - 26, 704, 32,
                COL(100, 150, 200, 200));
        }

        if(i==current_part_index) vita2d_pgf_draw_text(g_font, px-20, py+i*32, COL(255,255,100,255),1.2f,">");
        vita2d_pgf_draw_text(g_font, px, py+i*32, color,1.1f,line);
    }

    if(parts_count>0 && current_part_index>=0 && current_part_index<parts_count){
        const PartitionInfo* p = &parts[current_part_index];
        char info_buf[128], tbuf[64], fbuf[64];
        ui_format_bytes(p->total_bytes, tbuf,sizeof(tbuf));
        ui_format_bytes(p->free_bytes, fbuf,sizeof(fbuf));
        snprintf(info_buf,sizeof(info_buf),
                 "Partition: %s  |  Free: %s / %s  |  Items: %d",
                 p->label, fbuf, tbuf, folders_count);
        vita2d_pgf_draw_text(g_font,24,180,COL(255,255,200,255),1.0f,info_buf);

        float usage = (float)(p->total_bytes - p->free_bytes) / (float)p->total_bytes;
        int usage_label_x=24, usage_label_y=200;
        vita2d_pgf_draw_text(g_font, usage_label_x, usage_label_y, COL(255,255,255,255),1.0f,"Usage:");
        int usage_label_width=vita2d_pgf_text_width(g_font,1.0f,"Usage:")+8;
        draw_bar(usage_label_x+usage_label_width, usage_label_y-12,900,12,usage,color_for_usage(usage));
    }

    float fx=24, fy=270;
    int row_height=28;
    const int bar_width = 900;

    if(folders_count <= g_max_visible) g_scroll_offset=0;
    else {
        if(current_folder_index<g_scroll_offset) g_scroll_offset=current_folder_index;
        if(current_folder_index>=g_scroll_offset+g_max_visible) g_scroll_offset=current_folder_index-g_max_visible+1;
    }
    int start=g_scroll_offset;
    int end=(folders_count<g_scroll_offset+g_max_visible)?folders_count:g_scroll_offset+g_max_visible;

    for(int i=start;i<end;i++){
        char name_buf[128], size_buf[64];
        int is_dir = (strchr(folders[i].name, '/') != NULL);

        snprintf(name_buf,sizeof(name_buf),"%2d. %s",i+1,folders[i].name);
        int name_pixel_width = vita2d_pgf_text_width(g_font, 1.0f, name_buf);

        ui_format_bytes(folders[i].size_bytes, size_buf, sizeof(size_buf));

        int text_x = fx, text_y = fy + (i - start) * row_height;
        int visible_index = i - start;

        if(i == current_folder_index) {
            vita2d_draw_rectangle(text_x - 8, text_y - 24,
                950, row_height + 8,
                COL(80, 80, 120, 120));
            vita2d_draw_rectangle(text_x - 10, text_y - 26,
                970, row_height + 12,
                COL(120, 140, 180, 180));
        }

        uint32_t name_color = is_dir ? COL(120, 200, 255, 255) : COL(255, 255, 255, 255);
        vita2d_pgf_draw_text(g_font, text_x, text_y, name_color, 1.0f, name_buf);

        int size_x = 800;
        int size_text_width = vita2d_pgf_text_width(g_font, 1.0f, size_buf);
        vita2d_pgf_draw_text(g_font, size_x - size_text_width, text_y, COL(180, 255, 180, 255), 1.0f, size_buf);

        float bar_x = size_x + 20;
        float bar_fill = 0;
        if(parts_count > 0 && current_part_index >= 0 && current_part_index < parts_count){
            uint64_t part_total = parts[current_part_index].total_bytes;
            if(part_total > 0) bar_fill = (float)folders[i].size_bytes / (float)part_total;
        }
        draw_bar(bar_x, text_y - 20, bar_width, 18, bar_fill, color_for_filter_label());
    }

    if(overlay_labels && overlay_count>0 && overlay_offset_x < 960){
        float ox = overlay_offset_x;
        float oy = 100;
        vita2d_draw_rectangle(ox-10, oy-30, 220, overlay_count*26 + 40, COL(0,0,0,160));
        vita2d_pgf_draw_text(g_font, ox, oy-20, COL(255,255,255,255), 1.0f, "Choose a filter");
        for(int i=0;i<overlay_count;i++){
            uint32_t col = (i==overlay_sel)?COL(255,255,0,255):COL(255,255,255,255);
            if(strcasecmp(overlay_labels[i],g_filter_label)==0) col = COL(0,255,0,255);
            vita2d_pgf_draw_text(g_font, ox, oy+i*26, col, 1.0f, overlay_labels[i]);
        }
    }

    if(delete_confirm_active && delete_confirm_name) {
        float dialog_x = 480 - 250, dialog_y = 272 - 50;
        float dialog_w = 500, dialog_h = 100;
        float dialog_center_x = dialog_x + dialog_w / 2.0f;
        const int max_filename_width = 460;

        for(int dy = 0; dy < dialog_h; dy++) {
            float alpha = (float)dy / (float)dialog_h;
            uint32_t bg_color = COL(
                (uint8_t)(30 + alpha * 40),
                (uint8_t)(30 + alpha * 40),
                (uint8_t)(50 + alpha * 30),
                250
            );
            vita2d_draw_rectangle(dialog_x, dialog_y + dy, dialog_w, 1, bg_color);
        }

        vita2d_draw_rectangle(dialog_x-3, dialog_y-3, dialog_w+6, dialog_h+6, COL(255,100,100,200));
        vita2d_draw_rectangle(dialog_x-2, dialog_y-2, dialog_w+4, dialog_h+4, COL(255,150,150,255));
        vita2d_draw_rectangle(dialog_x-1, dialog_y-1, dialog_w+2, dialog_h+2, COL(255,200,200,255));

        float title_width = vita2d_pgf_text_width(g_font, 1.2f, "Delete this file/folder?");
        vita2d_pgf_draw_text(g_font, dialog_center_x - title_width/2.0f, dialog_y + 20, COL(255,100,100,255), 1.2f, "Delete this file/folder?");

        char display_filename[256];
        float filename_width = vita2d_pgf_text_width(g_font, 1.0f, delete_confirm_name);
        if(filename_width > max_filename_width) {
            strcpy(display_filename, delete_confirm_name);
            int len = strlen(display_filename);
            while(len > 3 && vita2d_pgf_text_width(g_font, 1.0f, display_filename) > max_filename_width - 24) {
                display_filename[--len] = '\0';
            }
            if(len > 3) {
                strcpy(display_filename + len - 3, "...");
            } else {
                strcpy(display_filename, "...");
            }
        } else {
            strcpy(display_filename, delete_confirm_name);
        }

        filename_width = vita2d_pgf_text_width(g_font, 1.0f, display_filename);
        vita2d_pgf_draw_text(g_font, dialog_center_x - filename_width/2.0f, dialog_y + 45, COL(255,255,200,255), 1.0f, display_filename);

        float buttons_width = vita2d_pgf_text_width(g_font, 1.0f, "X: Yes     O: Cancel");
        vita2d_pgf_draw_text(g_font, dialog_center_x - buttons_width/2.0f, dialog_y + 75, COL(150,255,150,255), 1.0f, "X: Yes");
        vita2d_pgf_draw_text(g_font, dialog_center_x - buttons_width/2.0f + vita2d_pgf_text_width(g_font, 1.0f, "X: Yes     "), dialog_y + 75, COL(255,150,150,255), 1.0f, "O: Cancel");
    }



    vita2d_end_drawing();
    vita2d_swap_buffers();
}
