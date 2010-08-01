#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: extend_file_12
 * Autor: Kevin Wolf
 * Zweck: Datei vergroessern (FAT 12)
 */

static void do_test(struct fat_fs* fs, const char* path)
{
    int ret;
    struct fat_file file;
    char buf[26000];
    int i;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }


    ret = fat_file_open(fs, path, &file);
    if (ret < 0) {
        test_error("Konnte Datei '%s' nicht oeffnen", path);
    }

    /* Ab 6500 bis 26000 'Hello World!' schreiben */
    for (i = 500; i < 2000; i++) {
        char* p = &buf[i * 13];
        memcpy(p, "Hello World!\n", 13);
    }

    ret = fat_file_write(&file, buf + 13 * 500, 13 * 500, 13 * 1500);
    if (ret < 0) {
        test_error("Konnte nicht schreiben (%d)", ret);
    }

    if (file.size != 26000) {
        test_error("Falsche Dateigroesse: %d", file.size);
    }

    /* Einmal die ganze Datei von vorne einlesen */
    memset(buf, 0xa5, 26000);
    ret = fat_file_read(&file, buf, 0, 26000);
    if (ret < 0) {
        test_error("Konnte nicht aus Datei lesen (%d)", ret);
    }

    for (i = 0; i < 2000; i++) {
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


        if (i < 500) {
            s = "Hallo heimur\n";
        } else {
            s = "Hello World!\n";
        }

        if (strncmp(p, s, 13)) {
            p[13] = '\0';
            test_error("[%d] Gelesen: '%s', erwartet '%s'", i, p, s);
        }
    }

    if (fat_file_close(&file) < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    /* Und jetzt nochmal die Datei neu oeffnen und dann probieren */
    ret = fat_file_open(fs, path, &file);
    if (ret < 0) {
        test_error("Konnte Datei '%s' nicht neu oeffnen", path);
    }

    memset(buf, 0xa5, 26000);
    ret = fat_file_read(&file, buf, 0, 26000);
    if (ret < 0) {
        test_error("Konnte nicht aus Datei lesen (%d)", ret);
    }

    for (i = 0; i < 2000; i++) {
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


        if (i < 500) {
            s = "Hallo heimur\n";
        } else {
            s = "Hello World!\n";
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
}

int run_test(struct fat_fs* fs)
{
    do_test(fs, "/test.txt");
    do_test(fs, "/dir/foo.txt");
    return 0;
}
