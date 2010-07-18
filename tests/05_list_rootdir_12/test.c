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

    ret = fat_dir_open(fs, "/", &dir);
    if (ret< 0) {
        test_error("Konnte Verzeichnis nicht oeffnen");
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

    ret = fat_dir_open(fs, "/", &dir);
    if (ret< 0) {
        test_error("Konnte Verzeichnis nicht oeffnen");
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
    if (ret != 2) {
        test_error("Erwartet: 2 Verzeichniseintraege, gelesen %d Eintraege\n",
            ret);
    }

    if (!dir_has_entry("/", "test.txt")) {
        test_error("Erwartete Datei fehlt: test.txt\n");
    }

    if (!dir_has_entry("/", "test")) {
        test_error("Erwartete Datei fehlt: test.\n");
    }

    if (dir_has_entry("/", "test.bla")) {
        test_error("Unerwartete Datei: test.bla\n");
    }

    if (dir_has_entry("/", ".txt")) {
        test_error("Unerwartete Datei: .txt\n");
    }

    return 0;
}
