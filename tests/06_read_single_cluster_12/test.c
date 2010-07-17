#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: read_single_cluster_12
 * Autor: Kevin Wolf
 * Zweck: Datei in einem einzelnen Cluster von Platte laden
 */
int run_test(struct fat_fs* fs)
{
    int ret;
    struct fat_file file;
    char buf[512];

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = fat_file_open(fs, "/test.txt", &file);
    if (ret < 0) {
        test_error("Konnte Datei /test.txt nicht oeffnen");
    }

    if (file.size != 13) {
        test_error("Datei /test.txt hat falsche Groesse: %d", file.size);
    }

    ret = fat_file_read(&file, buf, 0, file.size);
    if (ret < 0) {
        test_error("Konnt nicht aus Datei lesen (%d)", ret);
    }

    if (strncmp(buf, "Hallo heimur\n", 13)) {
        test_error("Gelesen: '%s'", buf);
    }

    if (fat_file_close(&file) < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    return 0;
}
