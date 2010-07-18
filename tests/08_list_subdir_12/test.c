#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: mount_fat12
 * Autor: Kevin Wolf
 * Zweck: Komplettes Wurzelverzeichnis einlesen
 */

static struct fat_fs* fs;

static int count_dir_entries(const char* path)
{
    struct fat_dir dir;
    struct fat_dir_entry dentry;
    int ret;
    int count;

    ret = fat_dir_open(fs, path, &dir);
    if (ret < 0) {
        test_error("Konnte Verzeichnis %s nicht oeffnen", path);
    }

    count = -1;
    do {
        count++;
        ret = fat_dir_read(&dir, &dentry);
    } while (ret >= 0);

    if (fat_dir_close(&dir) < 0) {
        test_error("Konnte Verzeichnis nicht schliessen");
    }

    return count;
}

static int dir_has_entry(const char* path, const char* name)
{
    struct fat_dir dir;
    struct fat_dir_entry dentry;
    int ret;
    int result = 0;

    ret = fat_dir_open(fs, path, &dir);
    if (ret < 0) {
        test_error("Konnte Verzeichnis %s nicht oeffnen", path);
    }

    do {
        ret = fat_dir_read(&dir, &dentry);
        if (ret >= 0 && !strcmp(dentry.name, name)) {
            result = 1;
            break;
        }
    } while (ret >= 0);

    if (fat_dir_close(&dir) < 0) {
        test_error("Konnte Verzeichnis nicht schliessen");
    }

    return result;
}

int run_test(struct fat_fs* _fs)
{
    int ret;

    fs = _fs;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = count_dir_entries("/");
    if (ret != 3) {
        test_error("Erwartet: 3 Verzeichniseintraege, gelesen %d Eintraege\n",
            ret);
    }

    ret = count_dir_entries("/dir");
    if (ret != 1) {
        test_error("Erwartet: 1 Verzeichniseintraege, gelesen %d Eintraege\n",
            ret);
    }

    ret = count_dir_entries("/dir/");
    if (ret != 1) {
        test_error("Erwartet: 1 Verzeichniseintraege, gelesen %d Eintraege\n",
            ret);
    }

    if (dir_has_entry("/dir/", "test.txt")) {
        test_error("Unerwartete Datei: test.txt\n");
    }

    if (dir_has_entry("/dir/", "test")) {
        test_error("Unerwartete Datei: test\n");
    }

    if (!dir_has_entry("/dir/", "foo")) {
        test_error("Datei fehlt: foo\n");
    }

    return 0;
}
