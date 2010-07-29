#include <stdio.h>
#include "fat.h"

/**
 * fat-Dateisystem einbinden. Dafuer muessen dev_read, dev_write und
 * dev_private (falls notwendig) initialisiert sein.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int fat_fs_mount(struct fat_fs* fs)
{
    // BPB einlesen
    if (!fs->dev_read(0, 512, &fs->bpb, fs->dev_private)) {
        return 0;
    }

    // Magic pruefen
    if (fs->bpb.magic != 0xaa55) {
        return 0;
    }

    // FAT-Variante bestimmen
    uint32_t total_sectors;
    uint32_t rootdir_sectors;
    uint32_t data_sectors;
    uint32_t data_clusters;

    if (fs->bpb.total_sectors != 0) {
        total_sectors = fs->bpb.total_sectors;
    } else {
        // TODO Hier ist auf jeden Fall FAT32, egal wie viele Cluster
        total_sectors = fs->bpb.total_sectors_large;
    }

    rootdir_sectors =
        (fs->bpb.rootdir_length * sizeof(struct fat_disk_dir_entry) + 511)
        / 512;

    // TODO fat_sectors ungueltig fuer FAT32
    data_sectors = total_sectors
        - fs->bpb.reserved_sectors
        - (fs->bpb.num_fats * fs->bpb.fat_sectors)
        - rootdir_sectors;
    data_clusters = data_sectors / fs->bpb.cluster_sectors;

    if (data_clusters < 4085) {
        fs->type = FS_TYPE_FAT12;
    } else if (data_clusters < 65525) {
        fs->type = FS_TYPE_FAT16;
    } else {
        fs->type = FS_TYPE_FAT32;
        data_sectors -=
            (fs->bpb.num_fats * fs->bpb.extended.fat32.fat_sectors);
    }


    fs->total_sectors = total_sectors;
    fs->first_data_cluster = total_sectors - data_sectors;

    return 1;
}

/**
 * fat-Dateisystem aushaengen. Dabei werden saemtliche gecachten Daten auf die
 * Platte geschrieben. Die Member sb und cache_handle in der fs-Struktur sind
 * danach NULL.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int fat_fs_unmount(struct fat_fs* fs)
{
    return 0;
}

/**
 * Saetmliche gecachten Daten auf die Platte schreiben
 */
void fat_fs_sync(struct fat_fs* fs)
{
}
