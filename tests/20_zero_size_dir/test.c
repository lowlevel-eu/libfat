#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: 20_zero_size_dir
 * Autor: Kevin Wolf
 * Zweck: Verzeichnisse der Groesse null sind zwar fehlerhaft, aber segfaulten
 * sollte man trotzdem nicht.
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
        return -1;
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

int run_test(struct fat_fs* _fs)
{
    int ret;

    fs = _fs;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = count_dir_entries("/");
    if (ret != 1) {
        test_error("Erwartet: 1 Verzeichniseintraege, gelesen %d Eintraege\n",
            ret);
    }

    ret = count_dir_entries("/test/");
    if (ret >= 0) {
        test_error("Erwartet: /test kann nicht geoeffnet werden\n",
            ret);
    }

    return 0;
}
