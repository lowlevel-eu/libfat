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
    dir->first_cluster = 0;
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
    dir->first_cluster = 0;
    dir->num = file.size / sizeof(struct fat_disk_dir_entry);

    return 0;
}

static int fat_subdir_open_by_dir_entry(struct fat_fs* fs,
    struct fat_dir_entry* dentry, struct fat_dir* subdir)
{
    struct fat_file file;
    void* buf;
    int first_cluster;
    int ret;

    ret = fat_file_open_by_dir_entry(fs, dentry, &file);
    if (ret < 0) {
        return ret;
    }

    /*
     * Ein Verzeichnis ohne Cluster ist ein korruptes FS, fehlschlagen ist also
     * in Ordnung. Wir sollten nur nicht auf file.blocklist[0] zugreifen.
     */
    if (file.blocklist == NULL) {
        fat_file_close(&file);
        return -FAT_ENOENT;
    }

    buf = malloc(file.size);
    first_cluster = file.blocklist[0];

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
    subdir->first_cluster = first_cluster;
    subdir->num = file.size / sizeof(struct fat_disk_dir_entry);

    return 0;
}

int fat_subdir_open(struct fat_fs* fs, struct fat_dir* dir, const char* path,
    struct fat_dir* subdir)
{
    struct fat_dir_entry dentry;
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
    return fat_subdir_open_by_dir_entry(fs, &dentry, subdir);
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

static void string_to_short_fn(struct fat_disk_dir_entry* entry,
    const char* buf)
{
    int i;
    const char* p;

    for (i = 0; i < 8; i++) {
        switch (buf[i]) {
            case '.':
                memset(&entry->name[i], ' ', 8 - i);
                p = &buf[i + 1];
                goto parse_ext;
            case '\0':
                memset(&entry->name[i], ' ', 8 - i);
                p = &buf[i];
                goto parse_ext;
            default:
                entry->name[i] = toupper(buf[i]);
                break;
        }
    }

    p = strchr(&buf[8], '.');
parse_ext:
    if (!p || !*p) {
        memset(entry->ext, ' ', 3);
        return;
    }

    for (i = 0; i < 3; i++) {
        if (p[i] == '\0') {
            memset(&entry->ext[i], ' ', 3 - i);
            return;
        }
        entry->ext[i] = toupper(p[i]);
    }
}

static int is_free_entry(struct fat_disk_dir_entry* entry)
{
    // Die Datei existiert nicht
    if (entry->name[0] == '\0') {
        return 1;
    }

    // Die Datei ist geloescht
    if (entry->name[0] == (char) 0xE5) {
        return 1;
    }

    return 0;
}

static int is_valid_entry(struct fat_disk_dir_entry* entry)
{
    // Die Datei existiert nicht oder ist geloescht
    if (is_free_entry(entry)) {
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

static void dir_entry_from_disk(struct fat_dir_entry* entry,
    struct fat_disk_dir_entry* disk_entry)
{
    short_fn_to_string(disk_entry, entry->name);

    entry->first_cluster        = disk_entry->first_cluster;
    entry->size                 = disk_entry->size;
    entry->attrib               = disk_entry->attrib;
}

static void dir_entry_to_disk(struct fat_file* file,
    struct fat_disk_dir_entry* disk_entry)
{
    string_to_short_fn(disk_entry, file->dir_entry.name);

    disk_entry->first_cluster   = file->dir_entry.first_cluster;
    disk_entry->size            = file->size;
    disk_entry->attrib          = file->dir_entry.attrib;
}

int fat_dir_read(struct fat_dir* dir, struct fat_dir_entry* entry)
{
    if (dir->i < 0 || dir->i >= dir->num) {
        return -1;
    }

    // Ungueltige Eintraege ueberspringen
    while (dir->i < dir->num && !is_valid_entry(&dir->entries[dir->i])) {
        short_fn_to_string(&dir->entries[dir->i], entry->name);
        dir->i++;
    }

    // Fertig?
    if (dir->i >= dir->num) {
        return -1;
    }

    dir_entry_from_disk(entry, &dir->entries[dir->i]);
    entry->dir_first_cluster    = dir->first_cluster;
    entry->index_in_dir         = dir->i;

    dir->i++;

    return 0;
}

int fat_dir_update_rootdir_entry(struct fat_file* file)
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

    dir_entry_to_disk(file, &dentry);

    if (!fs->dev_write(offset, sizeof(struct fat_disk_dir_entry), &dentry,
        fs->dev_private))
    {
        return -FAT_EIO;
    }

    return 0;
}

int fat_dir_update_subdir_entry(struct fat_file* file)
{
    struct fat_fs* fs = file->fs;
    struct fat_dir_entry dentry = {
        .first_cluster = file->dir_entry.dir_first_cluster,
        .size = 0,
        .attrib = FAT_ATTRIB_DIR,
    };
    struct fat_disk_dir_entry file_dentry;
    struct fat_file dir;
    int ret;

#if 0
    if (i < 0 ||i >= dir->num) {
        return -FAT_EINVAL;
    }
#endif

    uint32_t offset =
        file->dir_entry.index_in_dir * sizeof(struct fat_disk_dir_entry);


    ret = fat_file_open_by_dir_entry(fs, &dentry, &dir);
    if (ret < 0) {
        return ret;
    }

    /* Bisherigen Verzeichniseintrag laden, falls vorhanden */
    if (offset < file->size) {
        ret = fat_file_read(&dir, &file_dentry, offset,
             sizeof(struct fat_disk_dir_entry));
        if (ret < 0) {
            return ret;
        }
    } else {
        memset(&file_dentry, 0, sizeof(file_dentry));
    }

    /* Felder aktualisieren */
    dir_entry_to_disk(file, &file_dentry);

    ret = fat_file_write(&dir, &file_dentry, offset,
        sizeof(struct fat_disk_dir_entry));
    if (ret < 0) {
        return ret;
    }

    fat_file_close(&dir);

    return 0;
}

int fat_dir_update_entry(struct fat_file* file)
{
    if (file->dir_entry.dir_first_cluster == 0) {
        return fat_dir_update_rootdir_entry(file);
    } else {
        return fat_dir_update_subdir_entry(file);
    }
}

/* FIXME Verzeichnis lesen nach create_entry
 * FIXME Zwei vergroessernde Aufrufe ueberschreiben sich gegenseitig
 * FIXME Verzeichnis erweitern auf neuen nicht genullten Cluster */
int fat_dir_create_entry(struct fat_dir* dir, const char* name, int attr,
    struct fat_dir_entry* dir_entry)
{
    int i;
    int ret;

    /* Dateistruktur faken, deren Verzeichniseintrag "geaendert" werden soll */
    struct fat_file file = {
        .fs = dir->fs,
        .size = 0,
        .dir_entry = {
            .dir_first_cluster = dir->first_cluster,
            .index_in_dir = 0,
            .first_cluster = 0,
            .attrib = attr,
        },
    };

    /* Freien Index im Verzeichnis suchen */
    for (i = 0; i < dir->num; i++) {
        if (is_free_entry(&dir->entries[i])) {
            goto found;
        }
    }

    /* Ausser dem Wurzelverzeichnis koennen wir alles vergroessern */
    if (dir->first_cluster != 0) {
        i = dir->num;
        goto found;
    }

    return -FAT_ENOENT;

found:
    strncpy(file.dir_entry.name, name, sizeof(file.dir_entry.name));
    file.dir_entry.name[sizeof(file.dir_entry.name) - 1] = '\0';
    file.dir_entry.index_in_dir = i;

    if (attr & FAT_ATTRIB_DIR) {
        fat_file_alloc_first_cluster(&file);
    }

    /* Ins Verzeichnis eintragen */
    ret = fat_dir_update_entry(&file);
    if (ret < 0) {
        return ret;
    }

    if (dir_entry) {
        *dir_entry = file.dir_entry;
    }

    return 0;
}

int fat_dir_create(struct fat_dir* dir, const char* name,
    struct fat_dir_entry* dir_entry)
{
    struct fat_file dir_file;
    struct fat_dir_entry dentry;
    int ret;
    uint32_t cluster_size = dir->fs->bpb.cluster_sectors * 512;
    uint8_t buf[cluster_size];
    struct fat_disk_dir_entry* disk_dentry;

    /* Verzeichnis anlegen */
    ret = fat_dir_create_entry(dir, name, FAT_ATTRIB_DIR, &dentry);
    if (ret < 0) {
        return ret;
    }

    /* Und einen genullten Cluster reservieren */
    ret = fat_file_open_by_dir_entry(dir->fs, &dentry, &dir_file);
    if (ret < 0) {
        return ret;
    }

    disk_dentry = (struct fat_disk_dir_entry*) buf;
    memset(buf, 0, cluster_size);
    disk_dentry[0] = (struct fat_disk_dir_entry) {
        .name           = ".       ",
        .ext            = "   ",
        .attrib         = FAT_ATTRIB_DIR,
        .first_cluster  = dentry.first_cluster,
        .size           = 0,
    };
    disk_dentry[1] = (struct fat_disk_dir_entry) {
        .name           = "..      ",
        .ext            = "   ",
        .attrib         = FAT_ATTRIB_DIR,
        .first_cluster  = dir->first_cluster,
        .size           = 0,
    };

    ret = fat_file_write(&dir_file, buf, 0, cluster_size);
    fat_file_close(&dir_file);

    if (dir_entry) {
        *dir_entry = dentry;
    }

    return ret;
}

int fat_dir_close(struct fat_dir* dir)
{
    free(dir->entries);
    return 0;
}
