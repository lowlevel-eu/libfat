#include <string.h>
#include <stdio.h>
#include <test.h>

#include "fat.h"

/**
 * Test: mount_fat12
 * Autor: Kevin Wolf
 * Zweck: FAT12-Image mounten
 */
int run_test(struct fat_fs* fs)
{
    if (!fat_fs_mount(fs)) {
        test_error("Konnte das Dateisystem nicht mounten");
    }

    if (fs->type != FS_TYPE_FAT12) {
        test_error("Dateisystemtyp %d != %d", fs->type, FS_TYPE_FAT12);
    }

    if (fs->total_sectors != 2880) {
        test_error("total_sectors = %d != %d", fs->total_sectors, 2880);
    }

    return 0;
}
