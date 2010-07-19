#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

#undef DEBUG

/**
 * Test: read_multi_cluster_16
 * Autor: Kevin Wolf
 * Zweck: Datei in mehreren Clustern von Platte laden
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

    /* Einmal die ganze Datei von vorne einlesen */
    ret = fat_file_read(&file, buf, 0, 13000);
    if (ret < 0) {
        test_error("[1] Konnte nicht aus Datei lesen (%d)", ret);
    }

    for (i = 0; i < 1000; i++) {
        char* p = &buf[i * 13];
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


        if (strncmp(p, "Hallo heimur\n", 13)) {
            p[13] = '\0';
            test_error("Gelesen: '%s'", p);
        }
    }

    /* Einmal ein Stueckchen aus der Mitte des ersten Clusters lesen */
    memcpy(buf, "xxxxxxxxxx", 10);
    ret = fat_file_read(&file, buf, 6, 6);
    if (ret < 0) {
        test_error("[2] Konnte nicht aus Datei lesen (%d)", ret);
    }
    if (strncmp(buf, "heimurxxxx", 10)) {
        buf[10] = '\0';
        test_error("Aus der Mitte des ersten Clusters gelesen: '%s'", buf);
    }

    /* Einmal ein Stueckchen aus der Mitte eines spaeteren Clusters lesen */
    memcpy(buf, "xxxxxxxxxx", 10);
    ret = fat_file_read(&file, buf, 6 + 1300, 6);
    if (ret < 0) {
        test_error("[2] Konnte nicht aus Datei lesen (%d)", ret);
    }
    if (strncmp(buf, "heimurxxxx", 10)) {
        buf[10] = '\0';
        test_error("Aus der Mitte gelesen: '%s'", buf);
    }

    /* Einmal nach Ende der Datei lesen */
    ret = fat_file_read(&file, buf, 100000, 10);
    if (ret >= 0) {
        test_error("[1] Konnte nach Ende der Datei lesen");
    }

    ret = fat_file_read(&file, buf, 13310, 10);
    if (ret >= 0) {
        test_error("[2] Konnte nach Ende der Datei lesen");
    }

    ret = fat_file_read(&file, buf, 12999, 10);
    if (ret >= 0) {
        test_error("[3] Konnte nach Ende der Datei lesen");
    }

    if (fat_file_close(&file) < 0) {
        test_error("Konnte Datei nicht schliessen");
    }

    return 0;
}
