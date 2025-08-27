#include "fs_analyzer.h"
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/devctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ---- internal helpers -------------------------------------------------------

static int get_fs_info(const char* mount, uint64_t* total, uint64_t* freeb)
{
    SceIoDevInfo info;
    memset(&info, 0, sizeof(info));
    int res = sceIoDevctl(mount, 0x3001, NULL, 0, &info, sizeof(info));
    if (res < 0) return res;
    if (total) *total = info.max_size;
    if (freeb) *freeb = info.free_size;
    return 0;
}

static int exists_path(const char* path)
{
    SceIoStat st; memset(&st, 0, sizeof(st));
    int r = sceIoGetstat(path, &st);
    return (r >= 0);
}

static int ends_with_case_insensitive(const char* name, const char* ext)
{
    if (!name || !ext) return 0;
    size_t ln = strlen(name);
    size_t le = strlen(ext);
    if (le == 0 || ln < le) return 0;

    // allow both "mp3" and ".mp3"
    const char* e = ext;
    if (ext[0] == '.') { e = ext + 1; le -= 1; if (ln < le) return 0; }

    // find last '.'
    const char* dot = strrchr(name, '.');
    if (!dot) return 0;
    dot++; // skip '.'

    // compare case-insensitive
    for (size_t i = 0; i < le; ++i) {
        char a = (char)tolower((unsigned char)dot[i]);
        char b = (char)tolower((unsigned char)e[i]);
        if (a != b) return 0;
    }
    return dot[le] == '\0';
}

// recursive accumulator; if extensions==NULL or ext_count==0 -> count all files
static uint64_t accumulate_path_size(const char* path, int depth,
                                     const char** extensions, int ext_count)
{
    if (depth > 16) return 0;

    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) {
        SceIoStat st; 
        if (sceIoGetstat(path, &st) >= 0) return st.st_size;
        return 0;
    }

    uint64_t total = 0;
    SceIoDirent de; memset(&de, 0, sizeof(de));
    while (sceIoDread(dfd, &de) > 0) {
        if (!strcmp(de.d_name, ".") || !strcmp(de.d_name, "..")) { memset(&de,0,sizeof(de)); continue; }
        char child[1024];
        snprintf(child, sizeof(child), "%s%s%s", path, (path[strlen(path)-1]=='/')? "":"/", de.d_name);

        if (SCE_S_ISDIR(de.d_stat.st_mode)) {
            total += accumulate_path_size(child, depth+1, extensions, ext_count);
        } else {
            if (extensions && ext_count > 0) {
                int match = 0;
                for (int i = 0; i < ext_count && extensions[i]; ++i) {
                    if (ends_with_case_insensitive(de.d_name, extensions[i])) { match = 1; break; }
                }
                if (match) total += de.d_stat.st_size;
            } else {
                total += de.d_stat.st_size;
            }
        }
        memset(&de, 0, sizeof(de));
    }
    sceIoDclose(dfd);
    return total;
}

// ---- public API -------------------------------------------------------------

int fs_detect_partitions(PartitionInfo out_list[], int *out_count)
{
    const char* names[] = {"ux0", "ur0", "uma0", "imc0"};
    const char* paths[] = {"ux0:/", "ur0:/", "uma0:/", "imc0:/"};

    int n = 0;
    for (int i = 0; i < 4; ++i) {
        PartitionInfo pi; memset(&pi, 0, sizeof(pi));
        pi.label = names[i];
        pi.path  = paths[i];
        pi.present = exists_path(paths[i]) ? 1 : 0;
        if (pi.present) {
            if (get_fs_info(paths[i], &pi.total_bytes, &pi.free_bytes) < 0) {
                pi.total_bytes = 0;
                pi.free_bytes  = 0;
            }
            out_list[n++] = pi;
        }
    }
    *out_count = n;
    return n > 0 ? 0 : -1;
}

int fs_top_entries_in_root(const char* root_path, FolderUsage* out, int max_items)
{
    if (!root_path || !out || max_items <= 0) return -1;

    SceUID dfd = sceIoDopen(root_path);
    if (dfd < 0) return -1;

    FolderUsage tmp[128];
    int count = 0;

    SceIoDirent de; memset(&de, 0, sizeof(de));
    while (sceIoDread(dfd, &de) > 0 && count < 128) {
        if (!strcmp(de.d_name, ".") || !strcmp(de.d_name, "..")) { memset(&de,0,sizeof(de)); continue; }
        char child[1024];
        snprintf(child, sizeof(child), "%s%s%s", root_path, (root_path[strlen(root_path)-1]=='/')? "":"/", de.d_name);
        FolderUsage fu; memset(&fu, 0, sizeof(fu));
        snprintf(fu.name, sizeof(fu.name), "%s", de.d_name);
        if (SCE_S_ISDIR(de.d_stat.st_mode)) {
            fu.size_bytes = accumulate_path_size(child, 0, NULL, 0);
        } else {
            fu.size_bytes = de.d_stat.st_size;
        }
        tmp[count++] = fu;
        memset(&de, 0, sizeof(de));
    }
    sceIoDclose(dfd);

    // sort by size desc
    for (int i = 0; i < count; ++i) {
        for (int j = i+1; j < count; ++j) {
            if (tmp[j].size_bytes > tmp[i].size_bytes) {
                FolderUsage t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t;
            }
        }
    }

    int outn = (count < max_items) ? count : max_items;
    for (int i = 0; i < outn; ++i) out[i] = tmp[i];
    return outn;
}

uint64_t fs_size_by_extension(const char* root_path, const char** extensions, int ext_count)
{
    if (!root_path) return 0;
    return accumulate_path_size(root_path, 0, extensions, ext_count);
}

void format_bytes(uint64_t bytes, char* out, int outsz)
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double val = (double)bytes;
    int u = 0;
    while (val >= 1024.0 && u < 4) { val /= 1024.0; ++u; }
    snprintf(out, outsz, u >= 2 ? "%.2f %s" : "%.0f %s", val, units[u]);
}
