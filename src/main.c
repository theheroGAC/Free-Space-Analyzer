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
#define CALCULATING_DELAY_MS 300

typedef enum { F_ALL=0, F_GAMES, F_MP3, F_OGG, F_PHOTO, F_VIDEO, F__COUNT } Filter;

// File extensions mapping
static const char* EXT_ALL[]   = { NULL };
static const char* EXT_GAMES[] = { ".iso",".cso",".pbp",".bin",NULL };
static const char* EXT_MP3[]   = { ".mp3", NULL };
static const char* EXT_OGG[]   = { ".ogg", NULL };
static const char* EXT_PHOTO[] = { ".jpg",".jpeg",NULL };
static const char* EXT_VIDEO[] = { ".mp4", NULL };

static const char** filter_to_ext(Filter f) {
    switch(f){
        case F_GAMES: return EXT_GAMES;
        case F_MP3: return EXT_MP3;
        case F_OGG: return EXT_OGG;
        case F_PHOTO: return EXT_PHOTO;
        case F_VIDEO: return EXT_VIDEO;
        default: return EXT_ALL;
    }
}

static const char* filter_to_label(Filter f) {
    switch(f){
        case F_GAMES: return "Games";
        case F_MP3: return "MP3";
        case F_OGG: return "OGG";
        case F_PHOTO: return "Photo";
        case F_VIDEO: return "Video";
        default: return "All";
    }
}

static int ext_array_count(const char** arr){ int n=0; while(arr&&arr[n])++n; return n; }

// ---- FPS calculation ----
static uint64_t prev_time = 0;
static float fps_val = 0.0f;

static void update_fps() {
    uint64_t now = sceKernelGetProcessTimeWide(); // microseconds since boot
    if (prev_time != 0) {
        uint64_t delta = now - prev_time;
        if (delta > 0) fps_val = 1000000.0f / (float)delta;
    }
    prev_time = now;
}

static int get_fps() { return (int)fps_val; }

// ---- main loop ----
int main(int argc, char* argv[]) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    ui_init();
    ui_show_splash(1500);

    PartitionInfo parts[8]; int parts_count=0;
    FolderUsage top[128]; int top_count=0;

    fs_detect_partitions(parts, &parts_count);

    int current_part = 0;
    int current_folder = 0;

    if(parts_count>0){
        top_count = fs_top_entries_in_root(parts[current_part].path, top, 128);
        if(top_count<0) top_count=0;
    }

    int running = 1, move_delay = 0, calculating = 0;
    int overlay_active = 0, overlay_sel = 0;

    const char* overlay_labels[F__COUNT] = { "All", "Games", "MP3", "OGG", "Photo", "Video" };

    SceCtrlData pad, old_pad={0};
    SceRtcTick last_switch_time;
    sceRtcGetCurrentTick(&last_switch_time);

    Filter cur_filter = F_ALL;
    ui_set_filter_label(filter_to_label(cur_filter));

    while(running){
        update_fps(); // update real FPS

        sceCtrlPeekBufferPositive(0,&pad,1);
        unsigned int pressed = pad.buttons & ~old_pad.buttons;

        if (pressed & SCE_CTRL_SQUARE) overlay_active = !overlay_active;

        if(overlay_active){
            if(pressed & SCE_CTRL_UP)   overlay_sel = (overlay_sel - 1 + F__COUNT) % F__COUNT;
            if(pressed & SCE_CTRL_DOWN) overlay_sel = (overlay_sel + 1) % F__COUNT;

            if(pressed & SCE_CTRL_CROSS){
                cur_filter = (Filter)overlay_sel;
                ui_set_filter_label(filter_to_label(cur_filter));
                calculating = 1;
                sceRtcGetCurrentTick(&last_switch_time);
                overlay_active = 0;
            }

            if(pressed & SCE_CTRL_CIRCLE){
                overlay_active = 0;
            }
        } else {
            if(move_delay>0) move_delay--;
            if(move_delay==0){
                if(pad.ly<128-STICK_THRESHOLD){ 
                    current_part=(current_part-1+parts_count)%parts_count; 
                    move_delay=MOVE_DELAY; 
                    calculating=1; 
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder=0;
                }
                if(pad.ly>128+STICK_THRESHOLD){ 
                    current_part=(current_part+1)%parts_count; 
                    move_delay=MOVE_DELAY; 
                    calculating=1; 
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder=0;
                }
            }

            if(pressed & SCE_CTRL_UP){ if(current_folder>0) current_folder--; }
            if(pressed & SCE_CTRL_DOWN){ if(current_folder<top_count-1) current_folder++; }

            if(pressed & SCE_CTRL_TRIANGLE) running = 0;
        }

        int battery = scePowerGetBatteryLifePercent();
        if(battery<0) battery=0; if(battery>100) battery=100;

        // Recalculate folder sizes if needed
        SceRtcTick now;
        sceRtcGetCurrentTick(&now);
        uint64_t ms_diff = (now.tick - last_switch_time.tick)/1000ULL;
        if(calculating && ms_diff>=CALCULATING_DELAY_MS){
            if(cur_filter==F_ALL){
                top_count = fs_top_entries_in_root(parts[current_part].path, top, 128);
                if(top_count<0) top_count=0;
            } else {
                const char** exts=filter_to_ext(cur_filter);
                uint64_t filtered_size = fs_size_by_extension(parts[current_part].path, exts, ext_array_count(exts));
                snprintf(top[0].name,sizeof(top[0].name),"%s total",filter_to_label(cur_filter));
                top[0].size_bytes=filtered_size;
                top_count=1;
            }
            calculating=0;
        }

        ui_draw(parts, parts_count, current_part, top, top_count,
                battery, get_fps(), calculating?1.0f:0.0f,
                current_folder, overlay_active, overlay_sel, overlay_labels, F__COUNT);

        old_pad = pad;
    }

    ui_deinit();
    sceKernelExitProcess(0);
    return 0;
}
