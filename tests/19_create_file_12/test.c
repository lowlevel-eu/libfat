#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: create_file_12
 * Autor: Kevin Wolf
 * Zweck: Datei erstellen (FAT 12)
 */

int run_test(struct fat_fs* fs)
{
    struct fat_dir dir;
    struct fat_file file;
    int ret;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    ret = fat_dir_open(fs, "/", &dir);
    if (ret < 0) {
        test_error("Konnte das Wurzelverzeichnis nicht oeffnen");
    }

    ret = fat_dir_create_entry(&dir, "neu.txt", 0);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht anlegen");
    }

    ret = fat_dir_close(&dir);
    if (ret < 0) {
        test_error("Konnte das Wurzelverzeichnis nicht schliessen");
    }

    ret = fat_file_open(fs, "/neu.txt", &file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht oeffnen");
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht schliessen");
    }

    test_fsck();

    return 0;
}
