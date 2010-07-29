#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: file_attr
 * Autor: Kevin Wolf
 * Zweck: Dateiattribute auslesen
 */

#define check(cond, msg) \
    if (!(cond)) { \
        test_error(msg); \
    }

static void check_dentry(struct fat_dir_entry* dentry)
{
    if (!strcmp(dentry->name, "test.txt")) {
        check(dentry->attrib == FAT_ATTRIB_ARCHIVE, "attr test.txt");
    } else if (!strcmp(dentry->name, "dir")) {
        check(dentry->attrib == FAT_ATTRIB_DIR, "attr dir");
    } else if (!strcmp(dentry->name, "test")) {
        check(dentry->attrib == FAT_ATTRIB_ARCHIVE, "attr test");
    } else {
        test_error("Unerwartete Datei");
    }
}

int run_test(struct fat_fs* fs)
{
    struct fat_dir dir;
    struct fat_dir_entry dentry;
    int ret, cnt;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    ret = fat_dir_open(fs, "/", &dir);
    if (ret < 0) {
        test_error("Konnte Verzeichnis %s nicht oeffnen", "/");
    }

    cnt = 0;
    do {
        ret = fat_dir_read(&dir, &dentry);
        if (ret >= 0) {
            check_dentry(&dentry);
            cnt++;
        }
    } while (ret >= 0);

    if (cnt != 3) {
        test_error("Falsche Anzahl Eintraege");
    }

    if (fat_dir_close(&dir) < 0) {
        test_error("Konnte Verzeichnis nicht schliessen");
    }

    return 0;
}
