/*
 * Copyright (c) 2008 The LOST Project. All rights reserved.
 *
 * This code is derived from software contributed to the LOST Project
 * by Antoine Kaufmann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed by the LOST Project
 *     and its contributors.
 * 4. Neither the name of the LOST Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <fat.h>
#include "test.h"
#include "cache.h"
#include "cdi_cache.h"

extern int image_fd;

/**
 * Blocks fuer den Cache einlesen
 *
 * @param cache Cache-Handle
 * @param block Blocknummer
 * @param count Anzahl der zu lesenden BLocks
 * @param dest  Pointer auf Puffer in dem die Daten abgelegt werde sollen.
 * @param prv   Pointer auf unser fs
 *
 * @return Anzahl der gelesenen Blocks
 */
static int read_block(struct cdi_cache* cache, uint64_t block, size_t count,
    void* dest, void* prv)
{
    uint64_t start = block * cache->block_size;

    if (lseek(image_fd, (off_t) start, SEEK_SET) == (off_t) -1) {
        return 0;
    }

    return read(image_fd, dest, count * cache->block_size) / cache->block_size;
}

/**
 * Blocks fuer den Cache schreiben
 *
 * @param cache Cache-Handle
 * @param block Blocknummer
 * @param count Anzahl der zu schreibenden Blocks
 * @param src   Pointer auf Puffer aus dem die Daten gelesen werden sollen.
 * @param prv   Pointer auf unser fs
 *
 * @return Anzahl der geschriebenen Blocks
 */
static int write_block(struct cdi_cache* cache, uint64_t block, size_t count,
    const void* src, void* prv)
{
    uint64_t start = block * cache->block_size;

    if (lseek(image_fd, (off_t) start, SEEK_SET) == (off_t) -1) {
        return 0;
    }

    return write(image_fd, src, count * cache->block_size) / cache->block_size;
}


void* cache_create(struct fat_fs* fs, size_t block_size)
{
    return cdi_cache_create(block_size, sizeof(struct fat_cache_block),
        read_block, write_block, fs);
}

void cache_destroy(void* handle)
{
    cdi_cache_destroy(handle);
}

void cache_sync(void* handle)
{
    if (!cdi_cache_sync(handle)) {
        puts("cache: Sync fehlgeschlagen");
    }
}

struct fat_cache_block* cache_block(void* handle, uint64_t block, int noread)
{
    struct cdi_cache* c = handle;
    struct cdi_cache_block* cdi_b;
    struct fat_cache_block* b;

    if (!(cdi_b = cdi_cache_block_get(c, block))) {
        return NULL;
    }

    b = cdi_b->private;
    b->opaque = cdi_b;
    b->data = cdi_b->data;
    b->cache = handle;
    b->number = block;
    return b;
}

void cache_block_dirty(struct fat_cache_block* b)
{
    struct cdi_cache* c = b->cache;
    struct cdi_cache_block* cdi_b = b->opaque;

    cdi_cache_block_dirty(c, cdi_b);
}

void cache_block_free(struct fat_cache_block* b, int dirty)
{
    struct cdi_cache* c = b->cache;
    struct cdi_cache_block* cdi_b = b->opaque;

    if (dirty) {
        cache_block_dirty(b);
    }

    cdi_cache_block_release(c, cdi_b);
}

