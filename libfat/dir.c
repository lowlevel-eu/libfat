#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "fat.h"

int fat_dir_open(struct fat_fs* fs, const char* path, struct fat_dir* dir)
{
    int64_t rootdir_offset = fs->bpb.reserved_sectors +
        (fs->bpb.num_fats * fs->bpb.fat_sectors);

    uint32_t rootdir_sectors =
        (fs->bpb.rootdir_length * sizeof(struct fat_disk_dir_entry) + 511)
        / 512;

    void* buf = malloc(512 * rootdir_sectors);

    // Wurzelverzeichnis einlesen
    if (!fs->dev_read(rootdir_offset * 512, rootdir_sectors * 512, buf,
        fs->dev_private))
    {
        goto fail;
    }

    // Rueckgabestruktur befuellen
    dir->entries = buf;
    dir->fs = fs;
    dir->i = 0;
    dir->num = rootdir_sectors * 512 / sizeof(struct fat_disk_dir_entry);

    return 0;

fail:
    free(buf);
    return -1;
}


static void short_fn_to_string(struct fat_disk_dir_entry* entry, char* buf)
{
    int i;
    int pos = -1;
    int ext_start;

    // 8 Zeichen Name in Kleinbuchstaben
    for (i = 0; i < 8; i++) {
        buf[i] = tolower(entry->name[i]);
        if (!isspace(buf[i])) {
            pos = i;
        }
    }

    // Punkt
    buf[++pos] = '.';

    // 3 Zeichen Erweiterung in Kleinbuchstaben
    ext_start = pos + 1;
    for (i = 0; i < 3; i++) {
        buf[ext_start + i] = tolower(entry->ext[i]);
        if (!isspace(buf[ext_start + i])) {
            pos = ext_start + i;
        }
    }

    // Nullterminieren
    buf[++pos] = '\0';
}

static int is_valid_entry(struct fat_disk_dir_entry* entry)
{
    // Die Datei existiert nicht
    if (entry->name[0] == '\0') {
        return 0;
    }

    // Die Datentraegerbezeichnung interessiert uns nicht
    if (entry->attrib & FAT_ATTRIB_VOLUME) {
        return 0;
    }

    return 1;
}

int fat_dir_read(struct fat_dir* dir, struct fat_dir_entry* entry)
{
    if (dir->i < 0 || dir->i >= dir->num) {
        return -1;
    }

    // Ungueltige Eintraege ueberspringen
    while (dir->i < dir->num && !is_valid_entry(&dir->entries[dir->i])) {
        dir->i++;
    }

    // Fertig?
    if (dir->i >= dir->num) {
        return -1;
    }

    short_fn_to_string(&dir->entries[dir->i], entry->name);

    entry->first_cluster = dir->entries[dir->i].first_cluster;
    entry->size = dir->entries[dir->i].size;

    dir->i++;
    return 0;
}

int fat_dir_close(struct fat_dir* dir)
{
    free(dir->entries);
    return 0;
}
