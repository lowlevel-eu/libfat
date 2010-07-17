#include <string.h>
#include <assert.h>

#include "fat.h"

#define NUM_CLUSTERS(x, cluster_size) \
    (((x) + (cluster_size) - 1) / (cluster_size))
#define SECTOR_BITS (512 * 8)

#define MIN(a,b) ((a) < (b) ? (a) : (b))

// FIXME Sektorgroesse != 512

static uint8_t* get_fat_buffer(struct fat_fs* fs, uint32_t sector)
{
    static uint8_t buf[1024];
    static struct fat_fs* old_fs = NULL;
    static uint32_t old_sector = 0;

    if (fs == old_fs && sector == old_sector) {
        return buf;
    }

    /* TODO Evtl. reicht memcpy und lesen nur eines Sektors */
    if (!fs->dev_read(sector * 512, 1024, buf, fs->dev_private)) {
        old_fs = NULL;
        return NULL;
    }

    old_fs = fs;
    old_sector = sector;

    return buf;
}

static uint32_t get_next_cluster(struct fat_fs* fs, uint32_t cluster)
{
    uint8_t* buf;
    uint32_t fat_bit_offset = (cluster * 12ULL);
    uint32_t fat_sector = fat_bit_offset / SECTOR_BITS;
    uint16_t value;

    fat_sector += fs->bpb.reserved_sectors;
    fat_bit_offset %= SECTOR_BITS;

    buf = get_fat_buffer(fs, fat_sector);
    if (buf == NULL) {
        return 0;
    }

    value = *((uint16_t*) &buf[fat_bit_offset / 8]);
    if ((fat_bit_offset & 7) == 0) {
        value &= 0xFFF;
    } else {
        value >>= 4;
    }

    return value;
}

int fat_file_open_by_dir_entry(struct fat_fs* fs, struct fat_dir_entry* entry,
    struct fat_file* file)
{
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint32_t i;

    file->fs = fs;
    file->size = entry->size;

    /* Blockliste anlegen und einlesen */
    file->blocklist_size = NUM_CLUSTERS(file->size, cluster_size);
    file->blocklist = malloc(file->blocklist_size * sizeof(uint32_t));

    file->blocklist[0] = entry->first_cluster;
    for (i = 1; i < file->blocklist_size; i++) {
        file->blocklist[i] = get_next_cluster(fs, file->blocklist[i - 1]);

        if (file->blocklist[i] == 0) {
            goto fail;
        }
    }

    return 0;

fail:
    free(file->blocklist);
    return -FAT_EIO;
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

static uint32_t file_get_cluster(struct fat_file* file, uint32_t file_cluster)
{
    if (file_cluster >= file->blocklist_size) {
        return 0;
    }

    return file->blocklist[file_cluster];
}

static int64_t get_data_cluster_offset(struct fat_fs* fs, uint32_t cluster_idx)
{
    uint32_t cluster = fs->first_data_cluster + (cluster_idx - 2);

    return (cluster * fs->bpb.cluster_sectors) * 512;
}

static int read_cluster(struct fat_fs* fs, void* buf, uint32_t cluster_idx)
{
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint32_t cluster_offset = get_data_cluster_offset(fs, cluster_idx);

    if (!fs->dev_read(cluster_offset, cluster_size, buf, fs->dev_private)) {
        return -FAT_EIO;
    }

    return 0;
}

static int read_cluster_part(struct fat_fs* fs, void* buf,
    uint32_t cluster_idx,  size_t offset, size_t len)
{
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint8_t cluster[cluster_size];
    int ret;

    assert (offset + len <= cluster_size);

    ret = read_cluster(fs, cluster, cluster_idx);
    if (ret < 0) {
        return ret;
    }

    memcpy(buf, cluster + offset, len);
    return 0;
}

int32_t fat_file_read(struct fat_file* file, void* buf, uint32_t offset,
    size_t len)
{
    struct fat_fs* fs = file->fs;
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint8_t* p = buf;
    size_t n = len;
    int ret;
    uint32_t file_cluster = offset / cluster_size;
    uint32_t disk_cluster;
    uint32_t offset_in_cluster;

    /* Bei Ueberlaenge Fehler zurueckgeben */
    if (offset + len > file->size) {
        return -FAT_EIO;
    }

    /* Angefangener Cluster am Anfang */
    offset_in_cluster = offset & (cluster_size - 1);
    if (offset_in_cluster) {
        uint32_t count = MIN(n, cluster_size - offset_in_cluster);

        disk_cluster = file_get_cluster(file, file_cluster++);
        if (disk_cluster == 0) {
            return -FAT_EIO;
        }

        ret = read_cluster_part(fs, p, disk_cluster, offset_in_cluster, count);
        if (ret < 0) {
            return ret;
        }

        p += count;
        n -= count;
    }

    /* Eine beliebige Anzahl von ganzen Clustern in der Mitte */
    while (n >= cluster_size) {
        disk_cluster = file_get_cluster(file, file_cluster++);
        if (disk_cluster == 0) {
            return -FAT_EIO;
        }

        ret = read_cluster(fs, p, disk_cluster);
        if (ret < 0) {
            return ret;
        }

        p += cluster_size;
        n -= cluster_size;
    }

    /* Und wieder ein angefangener Cluster am Ende */
    if (n) {
        disk_cluster = file_get_cluster(file, file_cluster);
        if (disk_cluster == 0) {
            return -FAT_EIO;
        }

        ret = read_cluster_part(fs, p, disk_cluster, 0, n);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int fat_file_close(struct fat_file* file)
{
    free(file->blocklist);
    return 0;
}
