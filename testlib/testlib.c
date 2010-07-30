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

#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>
#include "test.h"

extern jmp_buf exit_test;
extern char* test_name;
extern char* image_name;
extern int image_fd;

__attribute__((noreturn)) void test_error(const char* fmt, ...)
{
    va_list valist;

    printf("\r[\033[01;31mfailed\033[00;00m] %s\n", test_name);
    printf("         ");
    va_start(valist, fmt);
    vprintf(fmt, valist);
    va_end(valist);
    puts("");

    longjmp(exit_test, 1);
}

__attribute__((noreturn)) void test_interror(const char* fmt, ...)
{
    va_list valist;

    printf("\r[\033[01;33merror \033[00;00m] %s\n", test_name);
    printf("         ");
    va_start(valist, fmt);
    vprintf(fmt, valist);
    va_end(valist);
    puts("");

    longjmp(exit_test, 2);
}

void test_fsck()
{
    char* cmdline;
    fsync(image_fd);

    asprintf(&cmdline, "cp %s tmp.img", image_name);
    system(cmdline);
    free(cmdline);

    asprintf(&cmdline, "/sbin/fsck.msdos -y tmp.img > /dev/null 2>&1");
    if (system(cmdline)) {
        test_error("fsck hat einen Fehler gemeldet");
    }
    free(cmdline);
}
