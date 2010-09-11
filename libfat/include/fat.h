#ifndef _FAT_H_
#define _FAT_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef CDI_HAS_ERRNO

#include <errno.h>
#define FAT_ENOENT  ENOENT
#define FAT_EIO     EIO
#define FAT_EINVAL  EINVAL
#define FAT_ENOSPC  ENOSPC

#else

#define FAT_ENOENT  1
#define FAT_EIO     2
#define FAT_EINVAL  3
#define FAT_ENOSPC  4

#endif

// TODO Nur fuer Debugging - spaeter entfernen
#include <stdio.h>

struct fat32_extended_bpb {
    uint32_t    fat_sectors;
    uint16_t    flags;
    uint16_t    version;
    uint32_t    rootdir_cluster;
} __attribute__((packed));

/**
 * BIOS Parameter Block
 */
struct fat_bpb {
    uint8_t     jmp_code[3];
    char        os_name[8];
    uint16_t    sector_size;
    uint8_t     cluster_sectors;
    uint16_t    reserved_sectors;
    uint8_t     num_fats;
    uint16_t    rootdir_length;
    uint16_t    total_sectors;
    uint8_t     media_type;

    /* Nur fuer FAT12/16; FAT32 benutzt fat32_extended_bpb.fat_sectors */
    uint16_t    fat_sectors;

    uint16_t    sectors_per_track;
    uint16_t    heads;
    uint32_t    hidden_sectors;
    uint32_t    total_sectors_large;
    union {
        struct fat32_extended_bpb   fat32;
        uint8_t                     bytes[474];
    } extended;
    uint16_t    magic;
} __attribute__((packed));

enum fat_type {
    FS_TYPE_FAT12,
    FS_TYPE_FAT16,
    FS_TYPE_FAT32,
};

/**
 * Zentrale Struktur zur Verwaltung eines eingebundenen Dateisystemes
 */
struct fat_fs {
    /**
     * Funktionspointer der von Aufrufer gesetzt werden muss. Diese Funktion
     * liest Daten vom Datentraeger ein, auf dem sich das Dateisystem befindet.
     *
     * @param start Position von der an gelesen werden soll
     * @param size  Anzahl der zu lesenden Bytes
     * @param dest  Pointer auf den Puffer in dem die Daten abgelegt werden
     *              sollen
     * @param prv   Private Daten zum Zugriff auf den Datentraeger (z.B.
     *              Dateideskriptor), aus dev_private zu entnehmen
     *
     * @return 1 wenn das Lesen geklappt hat, 0 sonst
     */
    int (*dev_read)(uint64_t start, size_t size, void* dest, void* prv);

    /**
     * Funktionspointer der von Aufrufer gesetzt werden muss. Diese Funktion
     * schreibt Daten auf den Datentraeger, auf dem sich das Dateisystem
     * befindet.
     *
     * @param start Position von der an gelesen werden soll
     * @param size  Anzahl der zu lesenden Bytes
     * @param dest  Pointer auf den Puffer, aus dem die Daten gelesen werden
     *              sollen.
     * @param prv   Private Daten zum Zugriff auf den Datentraeger (z.B.
     *              Dateideskriptor), aus dev_private zu entnehmen
     *
     * @return 1 wenn das Schreiben geklappt hat, 0 sonst
     */
    int (*dev_write)(uint64_t start, size_t size, const void* source,
        void* prv);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * erstellt einen neuen Cache und gibt ein Handle darauf zurueck.
     *
     * @param fs            Pointer auf das Dateisystem fuer das der Cache
     *                      erstellt werden soll.
     * @param block_size    Blockgroesse im Cache
     *
     * @return Handle oder NULL im Fehlerfall
     */
    void* (*cache_create)(struct fat_fs* fs, size_t block_size);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * zerstoert einen mit cache_create erstellten Cache
     *
     * @param cache Handle
     */
    void (*cache_destroy)(void* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * schreibt alle veraenderten Cache-Blocks auf die Platte
     *
     * @param cache Handle
     */
    void (*cache_sync)(void* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * holt einen Block aus dem Cache und gibt einen Pointer auf ein
     * Block-Handle zrueck. Dieser Pointer ist mindestens solange gueltig, wie
     * das Handle nicht freigegeben wird.
     *
     * @param cache     Cache-Handle
     * @param block     Blocknummer
     * @param noread    Wenn dieser Parameter != 0 ist, wird der Block nicht
     *                  eingelesen. Das kann benutzt werden, wenn er eh
     *                  vollstaendig ueberschrieben wird.
     *
     * @return Block-Handle
     */
    struct fat_cache_block* (*cache_block)(void* cache, uint64_t block,
        int noread);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * markiert den angegebenen Block als veraendert, damit er geschrieben wird,
     * bevor er aus dem Cache entfernt wird.
     *
     * @param handle Block-Handle
     */
    void (*cache_block_dirty)(struct fat_cache_block* handle);

    /**
     * Funktionspointer der vom Aufrufer gesetzt werden muss. Diese Funktion
     * gibt einen mit cache_block alloziierten Block wieder frei.
     *
     * @param handle    Block-Handle
     * @param dirty     Wenn != 0 wird der Block auch als Dirty markiert
     */
    void (*cache_block_free)(struct fat_cache_block* handle, int dirty);


    /// Private Daten zum Zugriff auf den Datentraeger
    void* dev_private;

    /// Handle fuer Blockcache
    void* cache_handle;

    /// Darf vom Aufrufer benutzt werden
    void* opaque;

    /// BIOS Parameter Block
    struct fat_bpb bpb;

    enum fat_type type;
    uint32_t total_sectors;
    uint32_t first_data_cluster;
};

/**
 * Cache-Block-Handle
 */
struct fat_cache_block {
    /// Blocknummer
    uint64_t number;

    /// Daten
    void* data;

    /// Cache-Handle
    void* cache;

    /// Daten fuer die Implementierung
    void* opaque;
};

/**
 * fat-Dateisystem einbinden. Dafuer muessen dev_read, dev_write und
 * dev_private (falls notwendig) initialisiert sein.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int fat_fs_mount(struct fat_fs* fs);

/**
 * fat-Dateisystem aushaengen. Dabei werden saemtliche gecachten Daten auf die
 * Platte geschrieben. Die Member sb und cache_handle in der fs-Struktur sind
 * danach NULL.
 *
 * @return 1 bei Erfolg, im Fehlerfall 0
 */
int fat_fs_unmount(struct fat_fs* fs);

/**
 * Saetmliche gecachten Daten auf die Platte schreiben
 */
void fat_fs_sync(struct fat_fs* fs);

#define FAT_ATTRIB_READONLY 0x01
#define FAT_ATTRIB_HIDDEN   0x02
#define FAT_ATTRIB_SYSTEM   0x04
#define FAT_ATTRIB_VOLUME   0x08
#define FAT_ATTRIB_DIR      0x10
#define FAT_ATTRIB_ARCHIVE  0x20

struct fat_disk_dir_entry {
    char name[8];
    char ext[3];
    uint8_t attrib;
    uint8_t data[14];
    uint16_t first_cluster;
    uint32_t size;
} __attribute__((packed));


struct fat_dir_entry {
    int index_in_dir;
    int dir_first_cluster;
    char name[13]; /* 8.3 plus \0 */
    int first_cluster;
    int attrib;
    uint32_t size;
};

struct fat_file;

struct fat_dir {
    struct fat_fs* fs;
    struct fat_disk_dir_entry* entries;
    int first_cluster;
    int i;
    int num;
};

int fat_dir_open(struct fat_fs* fs, const char* path, struct fat_dir* dir);
int fat_dir_read(struct fat_dir* dir, struct fat_dir_entry* entry);
int fat_dir_create_entry(struct fat_dir* dir, const char* name, int attr,
    struct fat_dir_entry* dir_entry);
int fat_dir_create(struct fat_dir* dir, const char* name,
    struct fat_dir_entry* dir_entry);
int fat_dir_update_entry(struct fat_file* file);
int fat_dir_close(struct fat_dir* dir);

struct fat_file {
    struct fat_fs* fs;
    uint32_t size;
    size_t blocklist_size;
    uint32_t* blocklist;
    struct fat_dir_entry dir_entry;
};

int fat_file_open(struct fat_fs* fs, const char* path, struct fat_file* file);
int fat_file_open_by_dir_entry(struct fat_fs* fs, struct fat_dir_entry* entry,
    struct fat_file* file);
int fat_file_alloc_first_cluster(struct fat_file* file);
int32_t fat_file_read(struct fat_file* file, void* buf, uint32_t offset,
    size_t len);
int32_t fat_file_write(struct fat_file* file, const void* buf, uint32_t offset,
    size_t len);
int fat_file_close(struct fat_file* file);

#endif
