#include <string.h>
#include <assert.h>

#include "fat.h"

#define NUM_CLUSTERS(x, cluster_size) \
    (((x) + (cluster_size) - 1) / (cluster_size))
#define SECTOR_BITS (512 * 8)

#define MIN(a,b) ((a) < (b) ? (a) : (b))

// FIXME Sektorgroesse != 512

static uint8_t fat_buf[1024];
static struct fat_fs* fat_buf_fs = NULL;
static uint32_t fat_buf_sector = 0;
static int fat_buf_changed = 0;

static uint32_t get_next_cluster(struct fat_fs* fs, uint32_t cluster);


static uint8_t* get_fat_buffer(struct fat_fs* fs, uint32_t sector)
{
    sector += fs->bpb.reserved_sectors;

    if (fs == fat_buf_fs && sector == fat_buf_sector) {
        return fat_buf;
    }

    /* FIXME Write back changed entries before overwriting them */
    /* TODO Evtl. reicht memcpy und lesen nur eines Sektors */
    if (!fs->dev_read(sector * 512, 1024, fat_buf, fs->dev_private)) {
        fat_buf_fs = NULL;
        return NULL;
    }

    fat_buf_fs = fs;
    fat_buf_sector = sector;
    fat_buf_changed = 0;

    return fat_buf;
}

static int commit_fat_buffer(struct fat_fs* fs)
{
    int i;
    uint32_t sector;

    if (fs != fat_buf_fs) {
        return 0;
    }

    sector = fat_buf_sector;
    for (i = 0; i < fs->bpb.num_fats; i++) {
        /* TODO Evtl. reicht schreiben nur eines Sektors */
        if (!fs->dev_write(sector * 512, 1024, fat_buf, fs->dev_private)) {
            return -FAT_EIO;
        }

        if (fs->type == FS_TYPE_FAT32) {
            sector += fs->bpb.extended.fat32.fat_sectors;
        } else {
            sector += fs->bpb.fat_sectors;
        }
    }

    fat_buf_changed = 0;

    return 0;
}

static uint32_t get_next_cluster_fat12(struct fat_fs* fs, uint32_t cluster)
{
    uint8_t* buf;
    uint32_t fat_bit_offset = (cluster * 12ULL);
    uint32_t fat_sector = fat_bit_offset / SECTOR_BITS;
    uint32_t value;

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

    if (value > 0xff5) {
        value |= 0xfffff000;
    }

    return value;
}

static uint32_t get_next_cluster_fat16(struct fat_fs* fs, uint32_t cluster)
{
    uint8_t* buf;
    uint32_t fat_sector = (cluster * sizeof(uint16_t)) / 512;
    uint32_t fat_offset = (cluster * sizeof(uint16_t)) % 512;
    uint32_t value;

    buf = get_fat_buffer(fs, fat_sector);
    if (buf == NULL) {
        return 0;
    }

    value = *((uint16_t*) &buf[fat_offset]);

    if (value > 0xfff5) {
        value |= 0xffff0000;
    }

    return value;
}

static uint32_t get_next_cluster_fat32(struct fat_fs* fs, uint32_t cluster)
{
    uint8_t* buf;
    uint32_t fat_sector = (cluster * sizeof(uint32_t)) / 512;
    uint32_t fat_offset = (cluster * sizeof(uint32_t)) % 512;
    uint32_t value;

    buf = get_fat_buffer(fs, fat_sector);
    if (buf == NULL) {
        return 0;
    }

    value = *((uint32_t*) &buf[fat_offset]);

    if (value > 0xffffff5) {
        value |= 0xf0000000;
    }

    return value;
}

static int set_next_cluster_fat12(struct fat_fs* fs, uint32_t cluster,
    uint32_t value)
{
    uint8_t* buf;
    uint32_t fat_bit_offset = (cluster * 12ULL);
    uint32_t fat_sector = fat_bit_offset / SECTOR_BITS;
    uint16_t* pvalue;

    fat_bit_offset %= SECTOR_BITS;

    buf = get_fat_buffer(fs, fat_sector);
    if (buf == NULL) {
        return 0;
    }

    pvalue = ((uint16_t*) &buf[fat_bit_offset / 8]);
    value &= 0xFFF;

    if ((fat_bit_offset & 7) == 0) {
        *pvalue &= ~0xFFF;
        *pvalue |= value;
    } else {
        *pvalue &= ~0xFFF0;
        *pvalue |= (value << 4);
    }
    fat_buf_changed = 1;

    get_next_cluster_fat12(fs, cluster);

    return 0;
}

static int set_next_cluster_fat16(struct fat_fs* fs, uint32_t cluster,
    uint32_t value)
{
    uint8_t* buf;
    uint32_t fat_sector = (cluster * sizeof(uint16_t)) / 512;
    uint32_t fat_offset = (cluster * sizeof(uint16_t)) % 512;

    buf = get_fat_buffer(fs, fat_sector);
    if (buf == NULL) {
        return -FAT_EIO;
    }

    *((uint16_t*) &buf[fat_offset]) = value;
    fat_buf_changed = 1;

    return 0;
}

static int set_next_cluster(struct fat_fs* fs, uint32_t cluster,
    uint32_t value)
{
    if (fs->type == FS_TYPE_FAT12) {
        return set_next_cluster_fat12(fs, cluster, value);
    } else if (fs->type == FS_TYPE_FAT16) {
        return set_next_cluster_fat16(fs, cluster, value);
    } else {
        return -FAT_EINVAL;
    }
}

int fat_file_alloc_first_cluster(struct fat_file* file)
{
    struct fat_fs* fs = file->fs;
    uint32_t num_data_clusters = NUM_CLUSTERS(
        fs->total_sectors - fs->first_data_cluster,
        fs->bpb.cluster_sectors);
    uint32_t j;
    int ret;

    /* Freien Cluster suchen und einhaengen */
    for (j = 0; j < num_data_clusters; j++) {
        if (get_next_cluster(fs, j) == 0) {
            goto found;
        }
    }

    return -FAT_ENOSPC;

    /* EOF-Markierung in die FAT eintragen */
found:
    ret = set_next_cluster(fs, j, 0xffffffff);
    if (ret < 0) {
        return ret;
    }

    commit_fat_buffer(fs);

    /* Cluster der Datei zuordnen */
    file->dir_entry.first_cluster = j;

    return 0;
}

static int fat_extend_file(struct fat_file* file, uint32_t new_size)
{
    struct fat_fs* fs = file->fs;
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    size_t old_blocklist_size = file->blocklist_size;
    size_t new_blocklist_size = NUM_CLUSTERS(new_size, cluster_size);
    int ret;

    /* Ersten Cluster der Kette allozieren */
    if (file->blocklist == NULL) {
        fat_file_alloc_first_cluster(file);

        old_blocklist_size = 1;
        file->blocklist_size = 1;
        file->blocklist = malloc(sizeof(*file->blocklist));
        file->blocklist[0] = file->dir_entry.first_cluster;
    }

    /* Wenn noetig, neue Cluster allozieren */
    if (new_blocklist_size != old_blocklist_size) {
        uint32_t i;
        uint32_t num_data_clusters = NUM_CLUSTERS(
            fs->total_sectors - fs->first_data_cluster,
            fs->bpb.cluster_sectors);
        uint32_t free_index = 0;

        /* Blockliste vergroessern */
        file->blocklist = realloc(file->blocklist,
            new_blocklist_size * sizeof(uint32_t));
        file->blocklist_size = new_blocklist_size;

        /* Neue Eintraege der Blockliste befuellen */
        for (i = old_blocklist_size; i < new_blocklist_size; i++) {
            uint32_t j;

            /* Freien Cluster suchen und einhaengen */
            for (j = free_index; j < num_data_clusters; j++) {
                if (get_next_cluster(fs, j) == 0) {

                    /* Neue EOF-Markierung */
                    file->blocklist[i] = j;
                    ret = set_next_cluster(fs, file->blocklist[i], 0xffffffff);
                    if (ret < 0) {
                        return ret;
                    }

                    /* In die Clusterkette einhaengen */
                    ret = set_next_cluster(fs, file->blocklist[i - 1], j);
                    if (ret < 0) {
                        return ret;
                    }

                    free_index = j;
                    goto found;
                }
            }

            return -FAT_ENOSPC;
        found:
            /* Weiter mit naechstem Cluster */;
        }

        /* Und zuletzt veraenderte Eintraege zurueckschreiben */
        commit_fat_buffer(fs);
    }

    /* Dateigroesse anpassen und ggf. neuen ersten Cluster eintragen */
    file->size = new_size;
    if ((file->dir_entry.attrib & FAT_ATTRIB_DIR) == 0) {
        fat_dir_update_entry(file);
    }

    return 0;
}

static uint32_t get_next_cluster(struct fat_fs* fs, uint32_t cluster)
{
    switch (fs->type) {
        case FS_TYPE_FAT12:
            return get_next_cluster_fat12(fs, cluster);

        case FS_TYPE_FAT16:
            return get_next_cluster_fat16(fs, cluster);

        case FS_TYPE_FAT32:
            return get_next_cluster_fat32(fs, cluster);

        default:
            return -FAT_EINVAL;
    }
}


int fat_file_open_by_dir_entry(struct fat_fs* fs, struct fat_dir_entry* entry,
    struct fat_file* file)
{
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint32_t cluster;
    uint32_t i;

    /* Dateistruktur anlegen */
    file->fs = fs;
    file->size = entry->size;
    file->dir_entry = *entry;

    /* Eine leere Datei braucht keine Blockliste */
    if (entry->first_cluster == 0) {
        file->blocklist = NULL;
        file->blocklist_size = 0;
        return 0;
    }

    /*
     * Blockliste anlegen und einlesen. Bei Groesse 0 (z.B. Verzeichnisse gehen
     * wir von unbekannter Dateigroesse aus und lassen die Blockliste dynamisch
     * wachsen.
     */
    if (file->size > 0) {
        file->blocklist_size = NUM_CLUSTERS(file->size, cluster_size);
    } else {
        file->blocklist_size = 1;
    }

    file->blocklist = malloc(file->blocklist_size * sizeof(uint32_t));
    file->blocklist[0] = entry->first_cluster;

    cluster = 0;
    i = 1;
    while (1) {
        cluster = get_next_cluster(fs, file->blocklist[i - 1]);

        /* Fehler und Dateiende */
        if (cluster == 0) {
            goto fail;
        } else if (cluster >= 0xfffffff8) {
            break;
        }

        /* Blocklist vergroessern, falls noetig */
        if (file->blocklist_size <= i) {
            file->blocklist_size *= 2;
            file->blocklist = realloc(file->blocklist,
                file->blocklist_size * sizeof(uint32_t));
        }

        /* In die Blockliste eintragen */
        file->blocklist[i] = cluster;

        i++;
    }

    /* Dateigroesse anpassen, wenn sie unbekannt war */
    if (file->size == 0) {
        file->size = i * cluster_size;
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
    char* p;

    /* Letzten Pfadtrenner suchen */
    p = strrchr(path, '/');
    if (p == NULL) {
        return -FAT_EINVAL;
    }

    /* Verzeichnis oeffnen */
    int len = p - path + 1;
    char dirname[len + 1];

    memcpy(dirname, path, len);
    dirname[len] = '\0';

    ret = fat_dir_open(fs, dirname, &dir);
    if (ret< 0) {
        return ret;
    }

    /* Alles vor dem Dateinamen wegwerfen */
    path = p + 1;

    /* Verzeichnis nach Dateinamen durchsuchen */
    do {
        ret = fat_dir_read(&dir, &dentry);
        if (ret >= 0 && !strcmp(dentry.name, path)) {
            break;
        }
    } while (ret >= 0);

    /* Datei oeffnen oder -ENOENT zurueckgeben, wenn sie nicht existiert */
    if (ret < 0) {
        return -FAT_ENOENT;
    }

    /* Verzeichnis wieder schliessen */
    ret = fat_dir_close(&dir);
    if (ret < 0) {
        return ret;
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
    uint32_t sector = fs->bpb.cluster_sectors * (cluster_idx - 2);

    return (fs->first_data_cluster + sector) * 512;
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

static int write_cluster(struct fat_fs* fs, void* buf, uint32_t cluster_idx)
{
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint32_t cluster_offset = get_data_cluster_offset(fs, cluster_idx);

    if (!fs->dev_write(cluster_offset, cluster_size, buf, fs->dev_private)) {
        return -FAT_EIO;
    }

    return 0;
}

static int write_cluster_part(struct fat_fs* fs, void* buf,
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

    memcpy(cluster + offset, buf, len);

    ret = write_cluster(fs, cluster, cluster_idx);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int32_t fat_file_rw(struct fat_file* file, void* buf, uint32_t offset,
    size_t len, int is_write)
{
    struct fat_fs* fs = file->fs;
    uint32_t cluster_size = fs->bpb.cluster_sectors * 512;
    uint8_t* p = buf;
    size_t n = len;
    int ret;
    uint32_t file_cluster = offset / cluster_size;
    uint32_t disk_cluster;
    uint32_t offset_in_cluster;

    /* Funktionspointer fuer Lesen/Schreiben von Clustern */
    int (*rw_cluster)(struct fat_fs* fs, void* buf, uint32_t cluster_idx);
    int (*rw_cluster_part)(struct fat_fs* fs, void* buf, uint32_t cluster_idx,
        size_t offset, size_t len);

    if (is_write) {
        rw_cluster      = write_cluster;
        rw_cluster_part = write_cluster_part;
    } else {
        rw_cluster      = read_cluster;
        rw_cluster_part = read_cluster_part;
    }

    /* Angefangener Cluster am Anfang */
    offset_in_cluster = offset & (cluster_size - 1);
    if (offset_in_cluster) {
        uint32_t count = MIN(n, cluster_size - offset_in_cluster);

        disk_cluster = file_get_cluster(file, file_cluster++);
        if (disk_cluster == 0) {
            return -FAT_EIO;
        }

        ret = rw_cluster_part(fs, p, disk_cluster, offset_in_cluster, count);
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

        ret = rw_cluster(fs, p, disk_cluster);
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

        ret = rw_cluster_part(fs, p, disk_cluster, 0, n);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int32_t fat_file_read(struct fat_file* file, void* buf, uint32_t offset,
    size_t len)
{

    /* Bei Ueberlaenge Fehler zurueckgeben */
    if (offset + len > file->size) {
        return -FAT_EIO;
    }

    return fat_file_rw(file, buf, offset, len, 0);
}

int32_t fat_file_write(struct fat_file* file, const void* buf, uint32_t offset,
    size_t len)
{
    int ret;

    /* Datei erweitern, falls noetig */
    if (offset + len > file->size) {
        ret = fat_extend_file(file, offset + len);
        if (ret < 0) {
            return ret;
        }
    }

    return fat_file_rw(file, (void*) buf, offset, len, 1);
}

int fat_file_close(struct fat_file* file)
{
    free(file->blocklist);
    return 0;
}
