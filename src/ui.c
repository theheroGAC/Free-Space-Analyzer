#include "ui.h"
#include <vita2d.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static vita2d_pgf* g_font = NULL;
static vita2d_texture* splash_tex = NULL;

static inline uint32_t COL(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return RGBA8(r, g, b, a);
}

void ui_init() {
    vita2d_init();
    g_font = vita2d_load_default_pgf();
    
    // Carica lo splash screen
    splash_tex = vita2d_load_PNG_file("app0:/assets/reihen.png"); // percorso corretto
}

void ui_deinit() {
    if (splash_tex) {
        vita2d_free_texture(splash_tex);
        splash_tex = NULL;
    }
    if (g_font) {
        vita2d_free_pgf(g_font);
        g_font = NULL;
    }
    vita2d_fini();
}

void ui_show_splash(int duration_ms) {
    if (!splash_tex) return;

    int screen_w = 960;
    int screen_h = 544;
    int tex_w = vita2d_texture_get_width(splash_tex);
    int tex_h = vita2d_texture_get_height(splash_tex);

    int steps = 20; // numero di step per fade
    int step_delay = duration_ms / (2 * steps); // metà tempo fade-in, metà fade-out

    // --- Fade-in ---
    for (int i = 0; i <= steps; i++) {
        float alpha = (float)i / steps;
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_tint(splash_tex, (screen_w - tex_w)/2, (screen_h - tex_h)/2, RGBA8(255,255,255,(int)(255*alpha)));
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceKernelDelayThread(step_delay * 1000);
    }

    // --- Mantieni visibile ---
    sceKernelDelayThread(step_delay * 1000 * steps);

    // --- Fade-out ---
    for (int i = steps; i >= 0; i--) {
        float alpha = (float)i / steps;
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_tint(splash_tex, (screen_w - tex_w)/2, (screen_h - tex_h)/2, RGBA8(255,255,255,(int)(255*alpha)));
        vita2d_end_drawing();
        vita2d_swap_buffers();
        sceKernelDelayThread(step_delay * 1000);
    }
}

static void draw_bar(float x, float y, float w, float h, float fill01) {
    if (fill01 < 0) fill01 = 0;
    if (fill01 > 1) fill01 = 1;
    vita2d_draw_rectangle(x - 1, y - 1, w + 2, h + 2, COL(255, 255, 255, 255));
    vita2d_draw_rectangle(x, y, w, h, COL(40, 40, 40, 255));
    vita2d_draw_rectangle(x, y, w * fill01, h, COL(90, 190, 90, 255));
}

void ui_draw(const PartitionInfo* parts, int parts_count, int current_index,
             const FolderUsage* top, int top_count,
             int battery_percent, int fps, float calc_alpha) {

    vita2d_start_drawing();
    vita2d_clear_screen();

    // --- Header ---
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "Free Space Analyzer | Battery: %d%% | FPS: %d", battery_percent, fps);
    vita2d_pgf_draw_text(g_font, 24, 40, COL(255, 255, 255, 255), 1.0f, hdr);

    // --- Partitions list ---
    float px = 24, py = 80;
    for (int i = 0; i < parts_count; ++i) {
        const PartitionInfo* p = &parts[i];
        char tbuf[64], fbuf[64], line[256];
        format_bytes(p->total_bytes, tbuf, sizeof(tbuf));
        format_bytes(p->free_bytes, fbuf, sizeof(fbuf));
        snprintf(line, sizeof(line), "%s:/  %s free / %s total", p->label, fbuf, tbuf);

        uint32_t color = (i == current_index) ? COL(90, 220, 90, 255) : COL(200, 200, 200, 255);

        if (i == current_index) {
            vita2d_pgf_draw_text(g_font, px - 20, py + i * 26, COL(255, 255, 0, 255), 1.0f, ">");
        }

        vita2d_pgf_draw_text(g_font, px, py + i * 26, color, 1.0f, line);
    }

    // --- Partition usage bar ---
    if (current_index >= 0 && current_index < parts_count) {
        const PartitionInfo* p = &parts[current_index];
        float bx = 24, by = 180, bw = 920, bh = 28;
        float used_ratio = 0.0f;
        if (p->total_bytes > 0 && p->free_bytes <= p->total_bytes) {
            used_ratio = (float)(p->total_bytes - p->free_bytes) / (float)p->total_bytes;
        }
        draw_bar(bx, by, bw, bh, used_ratio);

        char label[256], usedbuf[64], freebuf[64], totbuf[64];
        format_bytes(p->total_bytes - p->free_bytes, usedbuf, sizeof(usedbuf));
        format_bytes(p->free_bytes, freebuf, sizeof(freebuf));
        format_bytes(p->total_bytes, totbuf, sizeof(totbuf));
        snprintf(label, sizeof(label), "%s:/ Used: %s | Free: %s | Total: %s",
                 p->label, usedbuf, freebuf, totbuf);
        vita2d_pgf_draw_text(g_font, bx, by - 10, COL(255, 255, 255, 255), 1.0f, label);
    }

    // --- Calculating message ---
    if (calc_alpha > 0.0f) {
        vita2d_pgf_draw_text(g_font, 600, 140, COL(90, 220, 90, 255), 1.0f, "Calculating... Please wait");
    }

    // --- Top folders ---
    vita2d_pgf_draw_text(g_font, 24, 240, COL(200, 220, 255, 255), 1.0f, "Top folders/files (root):");
    int max_display = top_count < 10 ? top_count : 10;
    for (int i = 0; i < max_display; ++i) {
        char line[512], sbuf[64];
        format_bytes(top[i].size_bytes, sbuf, sizeof(sbuf));
        snprintf(line, sizeof(line), "%2d. %-32s  %12s", i + 1, top[i].name, sbuf);
        vita2d_pgf_draw_text(g_font, 24, 270 + i * 24, COL(230, 230, 230, 255), 1.0f, line);
    }

    // --- Commands info ---
    vita2d_pgf_draw_text(g_font, 24, 540, COL(180, 180, 180, 255), 1.0f,
                         "Stick Up/Down: change partition | X: exit");

    vita2d_end_drawing();
    vita2d_swap_buffers();
}
