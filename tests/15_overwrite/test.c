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
    char buf[13000];
    int i;

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
        "Hallo heimur\n", 4 * 13))
    {
        test_error("Gelesen: '%s'", buf);
    }

    /* Etwas grossflaechiger ueberschreiben */
    for (i = 100; i < 800; i++) {
        char* p = &buf[i * 13];
        memcpy(p, "Hello World!\n", 13);
    }

    ret = fat_file_write(&file, buf + 13 * 100, 13 * 100, 13 * 700);
    if (ret < 0) {
        test_error("Konnte nicht schreiben (%d)", ret);
    }

    /* Einmal die ganze Datei von vorne einlesen */
    memset(buf, 0xa5, 13000);
    ret = fat_file_read(&file, buf, 0, 13000);
    if (ret < 0) {
        test_error("Konnte nicht aus Datei lesen (%d)", ret);
    }

    for (i = 0; i < 1000; i++) {
        char* p = &buf[i * 13];
        const char* s;

#ifdef DEBUG
        int j;
        fprintf(stderr, "%5d / %5x:  ", i, i * 13);
        for (j = 0; j < 13; j++)
            fprintf(stderr, "%02x ", p[j]);
        fprintf(stderr, "  | ");
        for (j = 0; j < 13; j++)
            fprintf(stderr, "%c", p[j] >= 32 ? p[j] : '.');
        fprintf(stderr, "\n");
#endif


        if (i >= 100 && i < 800) {
            s = "Hello World!\n";
        } else {
            s = "Hallo heimur\n";
        }

        if (strncmp(p, s, 13)) {
            p[13] = '\0';
            test_error("[%d] Gelesen: '%s', erwartet '%s'", i, p, s);
        }
    }

    if (fat_file_close(&file) < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    test_fsck();

    return 0;

}
