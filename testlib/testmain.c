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
#include <fat.h>
#include <fcntl.h>
#include <setjmp.h>
#include <unistd.h>
#include <signal.h>

#include "test.h"
#include "cache.h"

jmp_buf exit_test;
char* test_name;
char* image_name;
int image_fd;

int dev_read(uint64_t start, size_t size, void* dest, void* prv)
{
    if (lseek(image_fd, (off_t) start, SEEK_SET) == (off_t) -1) {
        return 0;
    }

    return (read(image_fd, dest, size) == size);
}

int dev_write(uint64_t start, size_t size, const void* dest, void* prv)
{
    if (lseek(image_fd, (off_t) start, SEEK_SET) == (off_t) -1) {
        return 0;
    }

//    fprintf(stderr, "dev_write: start = %8lx, size = %4ld \n", start, size);
    return (write(image_fd, dest, size) == size);
}

static void sigsegv_handler(int sig)
{
    test_interror("Segfault");
}

extern int run_test(struct fat_fs* fs);
int main(int argc, char* argv[])
{
    struct fat_fs fs;

    fs.dev_read = dev_read;
    fs.dev_write = dev_write;
    fs.dev_private = NULL;

    fs.cache_create = cache_create;
    fs.cache_destroy = cache_destroy;
    fs.cache_sync = cache_sync;
    fs.cache_block = cache_block;
    fs.cache_block_dirty = cache_block_dirty;
    fs.cache_block_free = cache_block_free;

    signal(SIGSEGV, sigsegv_handler);

    test_name = argv[0];
    image_name = argv[1];
    printf("Test %s...", test_name);

    switch (setjmp(exit_test)) {
        case 0:
            image_fd = open(argv[1], O_RDWR);
            if (image_fd == -1) {
                test_interror("Fehler beim Oeffnen des Images");
            }

            run_test(&fs);
            printf("\r[\033[01;32mpassed\033[00;00m] %s\n", argv[0]);
            return 0;

        case 1:
            return 1;

        case 2:
            return 2;
    }

    // Wird nie erreicht
    printf("\nBUG: Das Ende von main() wurde erreicht\n\n");
    return -1;
}

