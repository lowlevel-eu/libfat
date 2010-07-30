#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: overwrite
 * Autor: Kevin Wolf
 * Zweck: Bereits vorhandene Cluster ueberschreiben
 */
int run_test(struct fat_fs* fs)
{
    int ret;
    struct fat_file file;
    char buf[130];

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = fat_file_open(fs, "/test.txt", &file);
    if (ret < 0) {
        test_error("Konnte Datei /test.txt nicht oeffnen");
    }

    /*
     * Datei ueberschreiben, neuer Inhalt:
     *
     * 101 x Hallo heimur
     *   2 x [Hallo Welt]
     * ...   Hallo heimur
     */
    memcpy(buf, "[Hallo Welt]\n[Hallo Welt]\n", 26);
    ret = fat_file_write(&file, buf, 101 * 13, 26);
    if (ret < 0) {
        test_error("Konnte nicht schreiben (%d)", ret);
    }

    /* Ein Stueck einlesen und pruefen */
    ret = fat_file_read(&file, buf, 1300, 4 * 13);
    if (ret < 0) {
        test_error("Konnte nicht aus Datei lesen (%d)", ret);
    }
    buf[4 * 13] = '\0';

    if (strncmp(buf, "Hallo heimur\n[Hallo Welt]\n[Hallo Welt]\n"
        "Hallo heimur\n", 13))
    {
        test_error("Gelesen: '%s'", buf);
    }

    if (fat_file_close(&file) < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    test_fsck();

    return 0;

}
