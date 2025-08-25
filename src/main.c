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

int main(int argc, char* argv[]) {
    // --- initialize analog stick ---
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    // --- optional overclock CPU/GPU ---
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    // --- initialize UI ---
    ui_init();

    // --- show splash screen for 1,5 secondi ---
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

    // --- calculating timer ---
    int calculating = 0;
    SceRtcTick last_switch_time;
    sceRtcGetCurrentTick(&last_switch_time);

    // --- exit safe flag ---
    int exiting = 0;

    while (running) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~old_pad.buttons;

        // --- exit logic ---
        if (!exiting && (pressed & SCE_CTRL_CROSS)) {
            exiting = 1;      // start exiting
            calculating = 0;   // stop calculating
        }

        // --- stick movement ---
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

            // start calculating if partition changed
            if (prev != current) {
                calculating = 1;
                sceRtcGetCurrentTick(&last_switch_time); // reset timer
            }
        }

        // --- battery level ---
        int battery = scePowerGetBatteryLifePercent();
        if (battery < 0) battery = 0;
        if (battery > 100) battery = 100;

        // --- FPS limiter ---
        static int frame_count = 0, fps = 30;
        frame_count++;
        if (frame_count >= 30) {
            frame_count = 0;
            fps = 30;
        }

        // --- calculate elapsed ms ---
        SceRtcTick now;
        sceRtcGetCurrentTick(&now);
        uint64_t ms_diff = (now.tick - last_switch_time.tick) / 1000ULL;

        // --- recalc top folders after 300ms ---
        if (calculating && ms_diff >= CALCULATING_DELAY_MS) {
            top_count = fs_top_entries_in_root(parts[current].path, top, 16);
            if (top_count < 0) top_count = 0;
            calculating = 0;
        }

        // --- draw UI ---
        ui_draw(parts, parts_count, current, top, top_count, battery, fps, calculating ? 1.0f : 0.0f);

        old_pad = pad;

        // --- finalize exit safely ---
        if (exiting) {
            vita2d_wait_rendering_done();  // wait GPU to finish
            running = 0;                   // exit only after GPU done
        }
    }

    ui_deinit();
    sceKernelExitProcess(0);
    return 0;
}
