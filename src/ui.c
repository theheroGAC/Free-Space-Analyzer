#include "ui.h"
#include "fs_analyzer.h"
#include <vita2d.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static vita2d_pgf* g_font = NULL;
static vita2d_texture* splash_tex = NULL;
static char g_filter_label[64] = "All";

// Scroll & display state
static int g_scroll_offset = 0;
static int g_max_visible = 10;

// Overlay animation state
static float overlay_offset_x = 960.0f; // start offscreen right
static int overlay_target = 0;           // 1 = visible, 0 = hidden
static const float overlay_speed = 15.0f;

// FPS real
static uint64_t g_last_time = 0;
static float g_fps_real = 0.0f;
static const float FPS_SMOOTH_FACTOR = 0.1f;

// Helper for RGBA color
static inline uint32_t COL(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return RGBA8(r, g, b, a); }

// Color for filter label
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

// Color for usage bar (green → yellow → red)
static uint32_t color_for_usage(float fill01) {
    if (fill01 < 0.5f) return COL(90, 220, 90, 255);
    if (fill01 < 0.8f) return COL(220, 220, 90, 255);
    return COL(220, 90, 90, 255);
}

// Format bytes for UI
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

// Initialize UI
void ui_init() {
    vita2d_init();
    g_font = vita2d_load_default_pgf();
    splash_tex = vita2d_load_PNG_file("app0:/assets/reihen.png");
    g_last_time = sceKernelGetSystemTimeWide();
}

// Free UI resources
void ui_deinit() {
    if (splash_tex) { vita2d_free_texture(splash_tex); splash_tex = NULL; }
    if (g_font)     { vita2d_free_pgf(g_font);         g_font = NULL;     }
    vita2d_fini();
}

// Show splash screen
void ui_show_splash(int duration_ms) {
    if (!splash_tex) return;
    const int screen_w = 960, screen_h = 544;
    int tex_w = vita2d_texture_get_width(splash_tex);
    int tex_h = vita2d_texture_get_height(splash_tex);
    int steps = 20;
    int step_delay = duration_ms / (2 * steps);

    for (int i = 0; i <= steps; i++) {
        float alpha = (float)i / steps;
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_tint(splash_tex, (screen_w - tex_w)/2, (screen_h - tex_h)/2,
                                 RGBA8(255,255,255,(int)(255*alpha)));
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceKernelDelayThread(step_delay * 1000);
    }
    sceKernelDelayThread(step_delay * 1000 * steps);
    for (int i = steps; i >= 0; i--) {
        float alpha = (float)i / steps;
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_tint(splash_tex, (screen_w - tex_w)/2, (screen_h - tex_h)/2,
                                 RGBA8(255,255,255,(int)(255*alpha)));
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceKernelDelayThread(step_delay * 1000);
    }
}

// Draw a usage/progress bar
static void draw_bar(float x, float y, float w, float h, float fill01, uint32_t fill_color) {
    if (fill01 < 0) fill01 = 0;
    if (fill01 > 1) fill01 = 1;
    vita2d_draw_rectangle(x - 1, y - 1, w + 2, h + 2, COL(255, 255, 255, 255));
    vita2d_draw_rectangle(x, y, w, h, COL(40, 40, 40, 255));
    vita2d_draw_rectangle(x, y, w * fill01, h, fill_color);
}

// ---- Update FPS reali ----
void ui_update_fps() {
    uint64_t now = sceKernelGetSystemTimeWide(); // microseconds
    if (g_last_time != 0) {
        float dt = (now - g_last_time) / 1000000.0f; // seconds
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
             const char* overlay_labels[], int overlay_count) {

    ui_update_fps(); // update FPS real

    // Animate overlay offset (right → left)
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

    // --- Header ---
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Free Space Analyzer | Battery: %d%% | FPS: %.1f", battery_percent, g_fps_real);
    vita2d_pgf_draw_text(g_font, 24, 36, COL(255,255,255,255), 1.0f, hdr);

    // --- "Calculating..." overlay with fade ---
    if(calc_alpha>0.0f){
        uint8_t alpha = (uint8_t)(255.0f * calc_alpha);
        vita2d_pgf_draw_text(g_font, 24, 60, COL(90,220,90,alpha), 1.0f, "Calculating... Please wait");
    }

    // --- Partitions ---
    float px = 24, py = 96;
    for(int i=0;i<parts_count;i++){
        const PartitionInfo* p = &parts[i];
        char tbuf[64], fbuf[64], line[256];
        ui_format_bytes(p->total_bytes, tbuf, sizeof(tbuf));
        ui_format_bytes(p->free_bytes, fbuf, sizeof(fbuf));
        snprintf(line,sizeof(line),"%s:/  %s free / %s total", p->label,fbuf,tbuf);
        uint32_t color = (i==current_part_index)?COL(90,220,90,255):COL(200,200,200,255);
        if(i==current_part_index) vita2d_pgf_draw_text(g_font, px-20, py+i*26, COL(255,255,0,255),1.0f,">");
        vita2d_pgf_draw_text(g_font, px, py+i*26, color,1.0f,line);
    }

    // --- Partition info bar ---
    if(parts_count>0 && current_part_index>=0 && current_part_index<parts_count){
        const PartitionInfo* p = &parts[current_part_index];
        char info_buf[128], tbuf[64], fbuf[64];
        ui_format_bytes(p->total_bytes, tbuf,sizeof(tbuf));
        ui_format_bytes(p->free_bytes, fbuf,sizeof(fbuf));
        snprintf(info_buf,sizeof(info_buf),
                 "Partition: %s  |  Free: %s / %s  |  Items: %d",
                 p->label, fbuf, tbuf, folders_count);
        vita2d_pgf_draw_text(g_font,24,240,COL(255,255,200,255),1.0f,info_buf);

        float usage = (float)(p->total_bytes - p->free_bytes) / (float)p->total_bytes;
        int usage_label_x=24, usage_label_y=260;
        vita2d_pgf_draw_text(g_font, usage_label_x, usage_label_y, COL(255,255,255,255),1.0f,"Usage:");
        int usage_label_width=vita2d_pgf_text_width(g_font,1.0f,"Usage:")+8;
        draw_bar(usage_label_x+usage_label_width, usage_label_y-12,200,12,usage,color_for_usage(usage));
    }

    // --- Folders list ---
    float fx=24, fy=300;
    int row_height=24;
    if(folders_count <= g_max_visible) g_scroll_offset=0;
    else {
        if(current_folder_index<g_scroll_offset) g_scroll_offset=current_folder_index;
        if(current_folder_index>=g_scroll_offset+g_max_visible) g_scroll_offset=current_folder_index-g_max_visible+1;
    }
    int start=g_scroll_offset;
    int end=(folders_count<g_scroll_offset+g_max_visible)?folders_count:g_scroll_offset+g_max_visible;
    int max_space_width=0;
    char space_buf[64];
    for(int i=start;i<end;i++){
        ui_format_bytes(folders[i].size_bytes,space_buf,sizeof(space_buf));
        int w=vita2d_pgf_text_width(g_font,1.0f,space_buf);
        if(w>max_space_width) max_space_width=w;
    }
    for(int i=start;i<end;i++){
        char name_buf[128], size_buf[64];
        snprintf(name_buf,sizeof(name_buf),"%2d. %-32s",i+1,folders[i].name);
        ui_format_bytes(folders[i].size_bytes,size_buf,sizeof(size_buf));
        int text_x=fx, text_y=fy+(i-start)*row_height;
        if(i==current_folder_index) vita2d_draw_rectangle(text_x-4,text_y-18,600,row_height,COL(40,40,60,200));
        vita2d_pgf_draw_text(g_font,text_x,text_y,COL(230,230,230,255),1.0f,name_buf);

        int space_x=fx+350;
        int space_text_width=vita2d_pgf_text_width(g_font,1.0f,size_buf);
        vita2d_pgf_draw_text(g_font,space_x+max_space_width-space_text_width,text_y,COL(200,255,200,255),1.0f,size_buf);

        float bar_x=space_x+max_space_width+8;
        float bar_w=100;
        float bar_fill=0;
        if(parts_count>0 && current_part_index>=0 && current_part_index<parts_count){
            uint64_t part_total=parts[current_part_index].total_bytes;
            if(part_total>0) bar_fill=(float)folders[i].size_bytes/(float)part_total;
        }
        draw_bar(bar_x,text_y-18,bar_w,16,bar_fill,color_for_filter_label());
    }

    // Overlay filters panel (right → left)
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

    // Command instructions menu
    vita2d_pgf_draw_text(g_font,24,540,COL(180,180,180,255),1.0f,
    "Up/Down: folder  |  Analog: partition  |  △: exit  |  □: toggle filters  |  X: confirm  |  O: back");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}