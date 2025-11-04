#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <vita2d.h>
#include <stdio.h>
#include <string.h>
#include "fs_analyzer.h"
#include "ui.h"

#define STICK_THRESHOLD 80
#define MOVE_DELAY 10
#define CALCULATING_DELAY_MS 300
#define MAX_PATH_LEN 512

typedef struct {
    char paths[16][MAX_PATH_LEN];
    int depth;
} Breadcrumb;

typedef enum { F_ALL=0, F_GAMES, F_MP3, F_OGG, F_PHOTO, F_VIDEO, F_DOCS, F_ARCHIVES, F_HOMEBREW, F_SAVEDATA, F__COUNT } Filter;

static const char* EXT_ALL[]      = { NULL };
static const char* EXT_GAMES[]    = { ".iso",".cso",".pbp",".bin",NULL };
static const char* EXT_MP3[]      = { ".mp3", NULL };
static const char* EXT_OGG[]      = { ".ogg", NULL };
static const char* EXT_PHOTO[]    = { ".jpg",".jpeg",".png",".bmp",".gif",NULL };
static const char* EXT_VIDEO[]    = { ".mp4",".avi",".mkv",".mov",NULL };
static const char* EXT_DOCS[]     = { ".txt",".pdf",".doc",".docx",".rtf",NULL };
static const char* EXT_ARCHIVES[] = { ".zip",".rar",".7z",".tar",".gz",NULL };
static const char* EXT_HOMEBREW[] = { ".vpk",".self",".suprx",".skprx",NULL };
static const char* EXT_SAVEDATA[] = { ".sav",".dat",".save",NULL };

static const char** filter_to_ext(Filter f) {
    switch(f){
        case F_GAMES: return EXT_GAMES;
        case F_MP3: return EXT_MP3;
        case F_OGG: return EXT_OGG;
        case F_PHOTO: return EXT_PHOTO;
        case F_VIDEO: return EXT_VIDEO;
        case F_DOCS: return EXT_DOCS;
        case F_ARCHIVES: return EXT_ARCHIVES;
        case F_HOMEBREW: return EXT_HOMEBREW;
        case F_SAVEDATA: return EXT_SAVEDATA;
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
        case F_DOCS: return "Docs";
        case F_ARCHIVES: return "Archives";
        case F_HOMEBREW: return "Homebrew";
        case F_SAVEDATA: return "SaveData";
        default: return "All";
    }
}

static int ext_array_count(const char** arr){ int n=0; while(arr&&arr[n])++n; return n; }

static int breadcrumb_init(Breadcrumb* bc, const char* initial_path) {
    if (!bc) return -1;
    bc->depth = 0;
    if (initial_path) {
        strncpy(bc->paths[0], initial_path, MAX_PATH_LEN - 1);
        bc->paths[0][MAX_PATH_LEN - 1] = '\0';
        bc->depth = 1;
    }
    return 0;
}

static int breadcrumb_push(Breadcrumb* bc, const char* path) {
    if (!bc || !path || bc->depth >= 15) return -1;
    strncpy(bc->paths[bc->depth], path, MAX_PATH_LEN - 1);
    bc->paths[bc->depth][MAX_PATH_LEN - 1] = '\0';
    bc->depth++;
    return 0;
}

static int breadcrumb_pop(Breadcrumb* bc) {
    if (!bc || bc->depth <= 1) return -1;
    bc->depth--;
    return 0;
}

static const char* breadcrumb_current(Breadcrumb* bc) {
    if (!bc || bc->depth <= 0) return "ux0:/";
    return bc->paths[bc->depth - 1];
}

static uint64_t prev_time = 0;
static float fps_val = 0.0f;

static void update_fps() {
    uint64_t now = sceKernelGetProcessTimeWide();
    if (prev_time != 0) {
        uint64_t delta = now - prev_time;
        if (delta > 0) fps_val = 1000000.0f / (float)delta;
    }
    prev_time = now;
}

static int get_fps() { return (int)fps_val; }

int main(int argc, char* argv[]) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    ui_init();

    PartitionInfo parts[8]; int parts_count=0;
    FolderUsage top[128]; int top_count=0;

    ui_draw(NULL, 0, 0, NULL, 0, 0, 0, 0.0f, 0, 0, 0, NULL, 0, 0, NULL, 1);

    fs_detect_partitions(parts, &parts_count);

    int current_part = 0;
    int current_folder = 0;

    Breadcrumb breadcrumb;
    memset(&breadcrumb, 0, sizeof(breadcrumb));

    if(parts_count > 0) {
        breadcrumb_init(&breadcrumb, parts[current_part].path);
        const char* current_path = breadcrumb_current(&breadcrumb);
        top_count = fs_scan_directory(current_path, top, 128);
        if(top_count<0) top_count=0;
    } else {
        breadcrumb_init(&breadcrumb, "ux0:/");
    }

    int running = 1, move_delay = 0, calculating = 0;
    int overlay_active = 0, overlay_sel = 0;
    int delete_confirm_active = 0;
    char delete_confirm_name[256] = "";

    const char* overlay_labels[F__COUNT] = { "All", "Games", "MP3", "OGG", "Photo", "Video", "Docs", "Archives", "Homebrew", "SaveData" };

    SceCtrlData pad, old_pad={0};
    SceRtcTick last_switch_time;
    sceRtcGetCurrentTick(&last_switch_time);

    Filter cur_filter = F_ALL;
    ui_set_filter_label(filter_to_label(cur_filter));

    while(running){
        update_fps();

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
        } else if(delete_confirm_active) {
            if(pressed & SCE_CTRL_CROSS) {
                char full_path[MAX_PATH_LEN];
                fs_build_path(breadcrumb_current(&breadcrumb), delete_confirm_name, full_path, sizeof(full_path));
                if (fs_delete_entry(full_path) == 0) {
                    calculating = 1;
                    sceRtcGetCurrentTick(&last_switch_time);
                }
                delete_confirm_active = 0;
            } else if(pressed & SCE_CTRL_CIRCLE) {
                delete_confirm_active = 0;
            }
        } else {
            if(move_delay>0) move_delay--;

            const char* current_path = breadcrumb_current(&breadcrumb);

            if(move_delay==0){
                if(pad.ly<128-STICK_THRESHOLD){
                    current_part=(current_part-1+parts_count)%parts_count;
                    move_delay=MOVE_DELAY;
                    calculating=1;
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder=0;
                    breadcrumb_init(&breadcrumb, parts[current_part].path);
                }
                if(pad.ly>128+STICK_THRESHOLD){
                    current_part=(current_part+1)%parts_count;
                    move_delay=MOVE_DELAY;
                    calculating=1;
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder=0;
                    breadcrumb_init(&breadcrumb, parts[current_part].path);
                }
            }

            if(pressed & SCE_CTRL_UP){ if(current_folder>0) current_folder--; }
            if(pressed & SCE_CTRL_DOWN){ if(current_folder<top_count-1) current_folder++; }

            if(pressed & SCE_CTRL_CROSS && current_folder < top_count) {
                const char* entry_name = top[current_folder].name;
                char new_path[MAX_PATH_LEN];
                fs_build_path(current_path, entry_name, new_path, sizeof(new_path));

                int is_dir = (entry_name[strlen(entry_name)-1] == '/') || fs_is_directory(new_path);

                if (is_dir) {
                    breadcrumb_push(&breadcrumb, new_path);
                    calculating = 1;
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder = 0;
                }
            }

            if(pressed & SCE_CTRL_CIRCLE) {
                if (breadcrumb.depth > 1) {
                    breadcrumb_pop(&breadcrumb);
                    calculating = 1;
                    sceRtcGetCurrentTick(&last_switch_time);
                    current_folder = 0;
                }
            }

            if(pressed & SCE_CTRL_TRIANGLE) running = 0;

            if(pressed & SCE_CTRL_RTRIGGER && current_folder < top_count && !delete_confirm_active) {
                const char* entry_name = top[current_folder].name;
                delete_confirm_active = 1;
                strncpy(delete_confirm_name, entry_name, sizeof(delete_confirm_name) - 1);
                delete_confirm_name[sizeof(delete_confirm_name) - 1] = '\0';
            }

        }

        int battery = scePowerGetBatteryLifePercent();
        if(battery<0) battery=0; if(battery>100) battery=100;

        SceRtcTick now;
        sceRtcGetCurrentTick(&now);
        uint64_t ms_diff = (now.tick - last_switch_time.tick)/1000ULL;
        if(calculating && ms_diff>=CALCULATING_DELAY_MS){
            const char* current_path = breadcrumb_current(&breadcrumb);
            if(cur_filter==F_ALL){
                top_count = fs_scan_directory(current_path, top, 128);
                if(top_count<0) top_count=0;
            } else {
                const char** exts=filter_to_ext(cur_filter);
                uint64_t filtered_size = fs_size_by_extension(current_path, exts, ext_array_count(exts));
                snprintf(top[0].name,sizeof(top[0].name),"%s total",filter_to_label(cur_filter));
                top[0].size_bytes=filtered_size;
                top_count=1;
            }
            calculating=0;
        }

        // Only draw UI if not exiting
        if (running) {
            ui_draw(parts, parts_count, current_part, top, top_count,
                    battery, get_fps(), calculating?1.0f:0.0f,
                    current_folder, overlay_active, overlay_sel, overlay_labels, F__COUNT,
                    delete_confirm_active, delete_confirm_name, 0);
        }

        old_pad = pad;
    }

    // Small delay to allow GPU to finish processing
    sceKernelDelayThread(100000); 
    ui_deinit();
    sceKernelExitProcess(0);
    return 0;
}
