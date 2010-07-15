#include <string.h>

#include "fat.h"

int fat_file_open_by_dir_entry(struct fat_fs* fs, struct fat_dir_entry* entry,
    struct fat_file* file)
{
    file->fs = fs;
    file->cluster = entry->first_cluster;
    file->size = entry->size;
    return 0;
}

int fat_file_open(struct fat_fs* fs, const char* path, struct fat_file* file)
{
    struct fat_dir dir;
    struct fat_dir_entry dentry;
    int ret;

    /* Verzeichnis oeffnen */
    ret = fat_dir_open(fs, "/", &dir);
    if (ret< 0) {
        return ret;
    }

    /*Einleitenden / entfernen */
    path++;

    /* Verzeichnis nach Dateinamen durchsuchen */
    do {
        ret = fat_dir_read(&dir, &dentry);
        if (ret >= 0 && !strcmp(dentry.name, path)) {
            break;
        }
    } while (ret >= 0);

    /* Verzeichnis wieder schliessen */
    ret = fat_dir_close(&dir);
    if (ret < 0) {
        return ret;
    }

    /* Datei oeffnen oder -ENOENT zurueckgeben, wenn sie nicht existiert */
    if (ret < 0) {
        return -FAT_ENOENT;
    }

    return fat_file_open_by_dir_entry(fs, &dentry, file);
}

static int64_t get_data_cluster_offset(struct fat_fs* fs, uint32_t cluster_idx)
{
    uint32_t cluster = fs->first_data_cluster + (cluster_idx - 2);

    return (cluster * fs->bpb.cluster_sectors) * 512;
}

int32_t fat_file_read(struct fat_file* file, void *buf, uint32_t offset,
    size_t len)
{
    struct fat_fs* fs = file->fs;
    uint32_t cluster_offset = get_data_cluster_offset(fs, file->cluster);

    if (!fs->dev_read(cluster_offset, len, buf, fs->dev_private)) {
        return -FAT_EIO;
    }


    return file->size;
}

int fat_file_close(struct fat_file* file)
{
    return 0;
}
