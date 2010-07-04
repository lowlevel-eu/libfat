#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: mount_fat12
 * Autor: Kevin Wolf
 * Zweck: FAT12-Image mounten
 */
int run_test(struct fat_fs* fs)
{
    int ret;
    struct fat_dir dir;
    struct fat_dir_entry dentry;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    ret = fat_dir_open(fs, "/", &dir);
    if (ret< 0) {
        test_error("Konnte Verzeichnis nicht oeffnen");
    }

    ret = fat_dir_read(&dir, &dentry);
    if (ret < 0) {
        test_error("Keine Datei gefunden");
    }
    if (strcmp(dentry.name, "test.txt")) {
        test_error("Erwartet: test.txt; gefunden: %s", dentry.name);
    }

    ret = fat_dir_read(&dir, &dentry);
    if (ret >= 0) {
        test_error("Zusaetzliche Datei: %s\n", dentry.name);
    }

    if (fat_dir_close(&dir) < 0) {
        test_error("Konnte Verzeichnis nicht schliessen");
    }

    return 0;
}
