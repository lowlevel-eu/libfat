#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: read_subdir_file
 * Autor: Kevin Wolf
 * Zweck: Datei in Unterverzeichnis auslesen
 */

int run_test(struct fat_fs* fs)
{
    int ret;
    struct fat_file file;
    char buf[256];

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = fat_file_open(fs, "/dir/foo", &file);
    if (ret < 0) {
        test_error("Konnte Datei nicht oeffnen");
    }

    ret = fat_file_read(&file, buf, 0, file.size);
    if (ret < 0) {
        test_error("Konnte nicht aus Datei lesen (%d)", ret);
    }

    buf[5] = '\0';
    if (strncmp(buf, "bar\n", 4)) {
        test_error("Gelesen: '%s'", buf);
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    return 0;
}
