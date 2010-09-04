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
    char buf[8192], buf2[8192];
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

    test_fsck();

    ret = fat_file_open(fs, "/neu.txt", &file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht oeffnen");
    }

    memset(buf, 0xa5, 8192);
    ret = fat_file_write(&file, buf, 0, 8192);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht schreiben");
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht schliessen");
    }

    test_fsck();

    ret = fat_file_open(fs, "/neu.txt", &file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht neu oeffnen");
    }

    ret = fat_file_read(&file, buf2, 0, 8192);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht lesen");
    }

    if (memcmp(buf, buf2, 8192)) {
        test_error("Daten stimmen nicht ueberein");
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht neu schliessen");
    }

    test_fsck();

    return 0;
}
