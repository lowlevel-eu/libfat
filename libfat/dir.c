#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "fat.h"

int fat_dir_open(struct fat_fs* fs, const char* path, struct fat_dir* dir)
{
    int64_t rootdir_offset = fs->bpb.reserved_sectors +
        (fs->bpb.num_fats * fs->bpb.fat_sectors);

    uint32_t rootdir_sectors =
        (fs->bpb.rootdir_length * sizeof(struct fat_dir_entry) + 511) / 512;

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
    dir->i = 1;
    dir->num = fs->bpb.rootdir_length * 512
        / sizeof(struct fat_disk_dir_entry);

    return 0;

fail:
    free(buf);
    return -1;
}


static void short_fn_to_string(struct fat_disk_dir_entry* entry, char* buf)
{
    int i;
    int pos = -1;

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
    for (i = 0, pos++; i < 3; i++) {
        buf[pos + i] = tolower(entry->ext[i]);
    }

    // Nullterminieren
    buf[pos + i] = '\0';
}

int fat_dir_read(struct fat_dir* dir, struct fat_dir_entry* entry)
{
    int i = dir->i;

    // TODO dir->num pruefen
#if 0
    while (dir->entries[i].attrib & FAT_ATTRIB_VOLUME) {
        i++;
    }
#endif

    if (i > 1) {
        return -1;
    }

    short_fn_to_string(&dir->entries[i], entry->name);

    dir->i++;
    return 0;
}

int fat_dir_close(struct fat_dir* dir)
{
    return 0;
}
