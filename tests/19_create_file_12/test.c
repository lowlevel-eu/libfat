#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: create_file_12
 * Autor: Kevin Wolf
 * Zweck: Datei erstellen (FAT 12)
 */

static struct fat_fs* _fs;

static int dir_has_entry(const char* path, const char* name)
{
    struct fat_fs* fs = _fs;
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

int run_test(struct fat_fs* fs)
{
    struct fat_dir dir;
    struct fat_file file;
    struct fat_dir_entry dentry;
    char buf[8192], buf2[8192];
    int ret;

    _fs = fs;

    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    /*** Neue Datei im Wurzelverzeichnis ***/

    /* Neue Datei anlegen */
    ret = fat_dir_open(fs, "/", &dir);
    if (ret < 0) {
        test_error("Konnte das Wurzelverzeichnis nicht oeffnen");
    }

    ret = fat_dir_create_entry(&dir, "neu.txt", 0, NULL);
    if (ret < 0) {
        test_error("Konnte neu.txt nicht anlegen");
    }

    ret = fat_dir_close(&dir);
    if (ret < 0) {
        test_error("Konnte das Wurzelverzeichnis nicht schliessen");
    }

    test_fsck();

    /* Dateiinhalt schreiben */
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

    /* Dateiinhalt testweise wieder lesen */
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



    /*** Neue Datei in einem Unterverzeichnis ***/

    /* Neue Datei anlegen */
    ret = fat_dir_open(fs, "/dir", &dir);
    if (ret < 0) {
        test_error("Konnte das Unterverzeichnis nicht oeffnen");
    }

    ret = fat_dir_create_entry(&dir, "moep", FAT_ATTRIB_ARCHIVE, &dentry);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht anlegen");
    }

    ret = fat_dir_close(&dir);
    if (ret < 0) {
        test_error("Konnte das Unterverzeichnis nicht schliessen");
    }

    test_fsck();

    if (dentry.dir_first_cluster != dir.first_cluster) {
        test_error("dentry.dir_first_cluster falsch");
    }
    if (strcmp(dentry.name, "moep")) {
        test_error("dentry.name falsch");
    }
    if (dentry.attrib != FAT_ATTRIB_ARCHIVE) {
        test_error("dentry.attrib falsch");
    }
    if (dentry.size != 0) {
        test_error("dentry.size falsch");
    }

    /* Dateiinhalt schreiben */
    ret = fat_file_open(fs, "/dir/moep", &file);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht oeffnen");
    }

    if (file.dir_entry.attrib != FAT_ATTRIB_ARCHIVE) {
        test_error("/dir/moep hat falsche Attribute");
    }

    memset(buf, 0x2a, 8192);
    ret = fat_file_write(&file, buf, 0, 8192);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht schreiben");
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht schliessen");
    }

    test_fsck();

    /* Dateiinhalt testweise wieder lesen */
    ret = fat_file_open(fs, "/dir/moep", &file);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht neu oeffnen");
    }

    ret = fat_file_read(&file, buf2, 0, 8192);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht lesen");
    }

    if (memcmp(buf, buf2, 8192)) {
        test_error("Daten in /dir/moep stimmen nicht ueberein");
    }

    ret = fat_file_close(&file);
    if (ret < 0) {
        test_error("Konnte /dir/moep nicht neu schliessen");
    }

    test_fsck();

    /*** Viele leere Dateien im Unterverzeichnis ***/
    char filename[2] = { '\0', '\0' };

    for (filename[0] = 'a'; filename[0] <= 'z'; filename[0]++) {
        ret = fat_dir_open(fs, "/dir", &dir);
        if (ret < 0) {
            test_error("Konnte das Unterverzeichnis nicht oeffnen");
        }

        ret = fat_dir_create_entry(&dir, filename, 0, NULL);
        if (ret < 0) {
            test_error("Konnte %s nicht anlegen", filename);
        }

        ret = fat_dir_close(&dir);
        if (ret < 0) {
            test_error("Konnte das Unterverzeichnis nicht schliessen");
        }
    }

    test_fsck();

    for (filename[0] = 'a'; filename[0] <= 'z'; filename[0]++) {
        if (!dir_has_entry("/dir", filename)) {
            test_error("Datei %s fehlt", filename);
        }
    }

    return 0;
}
