#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "fat.h"

static int fat_rootdir_open(struct fat_fs* fs, struct fat_dir* dir)
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

static int fat_rootdir32_open(struct fat_fs* fs, struct fat_dir* dir)
{
    struct fat_file file;
    struct fat32_extended_bpb* ext = &fs->bpb.extended.fat32;
    struct fat_dir_entry dentry = {
        .size = 0,
        .first_cluster = ext->rootdir_cluster,
    };
    int ret;
    void* buf;

    ret = fat_file_open_by_dir_entry(fs, &dentry, &file);
    if (ret < 0) {
        return ret;
    }

    buf = malloc(file.size);

    ret = fat_file_read(&file, buf, 0, file.size);
    fat_file_close(&file);
    if (ret < 0) {
        free(buf);
        return ret;
    }

    /* Rueckgabestruktur befuellen */
    dir->entries = buf;
    dir->fs = fs;
    dir->i = 0;
    dir->num = file.size / sizeof(struct fat_disk_dir_entry);

    return 0;
}

int fat_subdir_open(struct fat_fs* fs, struct fat_dir* dir, const char* path,
    struct fat_dir* subdir)
{
    struct fat_dir_entry dentry;
    struct fat_file file;
    void* buf;
    int ret;

    /* Vaterverzeichnis nach Unterverzeichnis durchsuchen */
    do {
        ret = fat_dir_read(dir, &dentry);
        if (ret < 0) {
            return ret;
        }

        if ((ret >= 0) && !strcmp(dentry.name, path)) {
            goto found;
        }
    } while (ret >= 0);

    return -FAT_ENOENT;

    /* Verzeichnis als Datei oeffnen und Eintraege laden */
found:
    ret = fat_file_open_by_dir_entry(fs, &dentry, &file);
    if (ret < 0) {
        return ret;
    }

    buf = malloc(file.size);

    ret = fat_file_read(&file, buf, 0, file.size);
    fat_file_close(&file);
    if (ret < 0) {
        free(buf);
        return ret;
    }

    /* Rueckgabestruktur befuellen */
    subdir->entries = buf;
    subdir->fs = fs;
    subdir->i = 0;
    subdir->num = file.size / sizeof(struct fat_disk_dir_entry);

    return 0;
}

int fat_dir_open(struct fat_fs* fs, const char* _path, struct fat_dir* dir)
{
    int ret;
    char* path = strdup(_path);
    char* p;
    struct fat_dir subdir;

    if (fs->type == FS_TYPE_FAT32) {
        ret = fat_rootdir32_open(fs, dir);
    } else {
        ret = fat_rootdir_open(fs, dir);
    }

    if (ret < 0) {
        goto out;
    }

    /* Pfade muessen mit einem Slash anfangen */
    if (*path != '/') {
        ret = -FAT_EINVAL;
        goto out;
    }

    /* Jetzt die Unterverzeichnisse parsen */
    p = strtok(path + 1, "/");
    while (p != NULL) {
        ret = fat_subdir_open(fs, dir, p, &subdir);
        fat_dir_close(dir);
        if (ret < 0) {
            goto out;
        }
        *dir = subdir;
        p = strtok(NULL, "/");
    }

    ret = 0;
out:
    free(path);
    return ret;
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
            pos = ext_start + i + 1;
        }
    }

    // Nullterminieren
    buf[pos] = '\0';
}

static int is_valid_entry(struct fat_disk_dir_entry* entry)
{
    // Die Datei existiert nicht
    if (entry->name[0] == '\0') {
        return 0;
    }

    // Die Datei ist geloescht
    if (entry->name[0] == (char) 0xE5) {
        return 0;
    }

    // Die Datentraegerbezeichnung interessiert uns nicht
    if (entry->attrib & FAT_ATTRIB_VOLUME) {
        return 0;
    }

    // . und .. werden ignoriert
    if (entry->name[0] == '.') {
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

    entry->index_in_dir = dir->i;
    entry->first_cluster = dir->entries[dir->i].first_cluster;
    entry->size = dir->entries[dir->i].size;
    entry->attrib = dir->entries[dir->i].attrib;

    dir->i++;
    return 0;
}

int fat_dir_update_entry(struct fat_file* file)
{
    struct fat_fs* fs = file->fs;
    struct fat_disk_dir_entry dentry;

#if 0
    if (i < 0 ||i >= dir->num) {
        return -FAT_EINVAL;
    }
#endif

    int64_t rootdir_offset = fs->bpb.reserved_sectors +
        (fs->bpb.num_fats * fs->bpb.fat_sectors);
    uint32_t offset = rootdir_offset * 512 +
        file->dir_entry.index_in_dir * sizeof(struct fat_disk_dir_entry);

    if (!fs->dev_read(offset, sizeof(struct fat_disk_dir_entry), &dentry,
        fs->dev_private))
    {
        return -FAT_EIO;
    }

    dentry.size = file->size;

    if (!fs->dev_write(offset, sizeof(struct fat_disk_dir_entry), &dentry,
        fs->dev_private))
    {
        return -FAT_EIO;
    }

    return 0;
}

int fat_dir_close(struct fat_dir* dir)
{
    free(dir->entries);
    return 0;
}
