#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: mount_fat32
 * Autor: Kevin Wolf
 * Zweck: FAT32-Image mounten
 */
int run_test(struct fat_fs* fs)
{
    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    if (fs->type != FS_TYPE_FAT32) {
        test_error("Dateisystemtyp %d != %d", fs->type, FS_TYPE_FAT32);
    }

    if (fs->total_sectors != 67584) {
        test_error("total_sectors = %d != %d", fs->total_sectors, 67584);
    }

    return 0;
}
