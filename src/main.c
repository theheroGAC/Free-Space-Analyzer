#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include "fs_analyzer.h"
#include "ui.h"

#define STICK_THRESHOLD 80
#define MOVE_DELAY 10
#define CALCULATING_DELAY_MS 300  // 300ms min display for "Calculating..."

typedef enum {
    F_ALL = 0,
    F_GAMES,
    F_MP3,
    F_OGG,
    F_JPG,
    F__COUNT
} Filter;

// extensions per filter (case-insensitive). NULL-terminated for safety.
static const char* EXT_ALL[]   = { NULL };
static const char* EXT_GAMES[] = { ".iso", ".cso", ".pbp", ".bin", NULL };
static const char* EXT_MP3[]   = { ".mp3", NULL };
static const char* EXT_OGG[]   = { ".ogg", NULL };
static const char* EXT_JPG[]   = { ".jpg", ".jpeg", NULL };

static const char** filter_to_ext(Filter f) {
    switch (f) {
        case F_GAMES: return EXT_GAMES;
        case F_MP3:   return EXT_MP3;
        case F_OGG:   return EXT_OGG;
        case F_JPG:   return EXT_JPG;
        case F_ALL:
        default:      return EXT_ALL;
    }
}
static const char* filter_to_label(Filter f) {
    switch (f) {
        case F_GAMES: return "Games";
        case F_MP3:   return "MP3";
        case F_OGG:   return "OGG";
        case F_JPG:   return "JPG";
        case F_ALL:
        default:      return "All";
    }
}
static int ext_array_count(const char** arr) { int n=0; while (arr && arr[n]) ++n; return n; }

int main(int argc, char* argv[]) {
    // init analog
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    // optional overclock
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    // init UI & splash
    ui_init();
    ui_show_splash(1500);

    PartitionInfo parts[8];
    int parts_count = 0;
    int current = 0;

    FolderUsage top[16];
    int top_count = 0;

    fs_detect_partitions(parts, &parts_count);
    if (parts_count > 0) {
        top_count = fs_top_entries_in_root(parts[current].path, top, 16);
        if (top_count < 0) top_count = 0;
    }

    int running = 1;
    SceCtrlData pad, old_pad = {0};
    int move_delay = 0;

    int calculating = 0;
    SceRtcTick last_switch_time;
    sceRtcGetCurrentTick(&last_switch_time);

    int exiting = 0;

    Filter cur_filter = F_ALL;
    ui_set_filter_label(filter_to_label(cur_filter));

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~old_pad.buttons;

        // exit
        if (!exiting && (pressed & SCE_CTRL_CROSS)) {
            exiting = 1;
            calculating = 0;
        }

        // filter changes (Option A in top bar):
        // L trigger -> All, Triangle -> Games, Square -> OGG, Circle -> MP3, R trigger -> JPG
        if (!exiting) {
            Filter prev_f = cur_filter;
            if (pressed & SCE_CTRL_LTRIGGER) cur_filter = F_ALL;
            if (pressed & SCE_CTRL_TRIANGLE) cur_filter = F_GAMES;
            if (pressed & SCE_CTRL_SQUARE)   cur_filter = F_OGG;
            if (pressed & SCE_CTRL_CIRCLE)   cur_filter = F_MP3;
            if (pressed & SCE_CTRL_RTRIGGER) cur_filter = F_JPG;
            if (prev_f != cur_filter) {
                ui_set_filter_label(filter_to_label(cur_filter));
                calculating = 1;
                sceRtcGetCurrentTick(&last_switch_time);
            }
        }

        // stick movement
        if (!exiting && move_delay > 0) move_delay--;
        int prev = current;
        if (!exiting && move_delay == 0) {
            if (pad.ly < 128 - STICK_THRESHOLD) { // up
                current = (current - 1 + parts_count) % parts_count;
                move_delay = MOVE_DELAY;
            }
            if (pad.ly > 128 + STICK_THRESHOLD) { // down
                current = (current + 1) % parts_count;
                move_delay = MOVE_DELAY;
            }
            if (prev != current) {
                calculating = 1;
                sceRtcGetCurrentTick(&last_switch_time);
            }
        }

        // battery clamped
        int battery = scePowerGetBatteryLifePercent();
        if (battery < 0) battery = 0;
        if (battery > 100) battery = 100;

        // FPS (simple placeholder like original)
        static int frame_count = 0, fps = 30;
        frame_count++;
        if (frame_count >= 30) { frame_count = 0; fps = 30; }

        // elapsed
        SceRtcTick now;
        sceRtcGetCurrentTick(&now);
        uint64_t ms_diff = (now.tick - last_switch_time.tick) / 1000ULL;

        // recalc after a small delay (keeps "Calculating..." visible and avoids GPU hitches)
        if (calculating && ms_diff >= CALCULATING_DELAY_MS) {
            if (cur_filter == F_ALL) {
                top_count = fs_top_entries_in_root(parts[current].path, top, 16);
                if (top_count < 0) top_count = 0;
            } else {
                // compute filtered total size; show it as a single row (keeps "Top 10" area free)
                const char** exts = filter_to_ext(cur_filter);
                uint64_t filtered_size = fs_size_by_extension(parts[current].path, exts, ext_array_count(exts));
                snprintf(top[0].name, sizeof(top[0].name), "%s total", filter_to_label(cur_filter));
                top[0].size_bytes = filtered_size;
                top_count = 1;
            }
            calculating = 0;
        }

        // draw
        ui_draw(parts, parts_count, current, top, top_count, battery, fps, calculating ? 1.0f : 0.0f);

        old_pad = pad;

        if (exiting) {
            vita2d_wait_rendering_done();
            running = 0;
        }
    }

    ui_deinit();
    sceKernelExitProcess(0);
    return 0;
}
