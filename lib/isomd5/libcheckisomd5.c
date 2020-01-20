/*
 * Copyright (C) 2001-2013 Red Hat, Inc.
 *
 * Michael Fulbright <msf@redhat.com>
 * Dustin Kirkland  <dustin.dirkland@gmail.com>
 *      Added support for checkpoint fragment sums;
 *      Exits media check as soon as bad fragment md5sum'ed
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>

#include "md5.h"
#include "libcheckisomd5.h"

#ifdef __APPLE__
#define lseek64 lseek
#endif

#ifdef _WIN32
size_t getpagesize () {
    return 2048; // not really necessary for Windows
}
#endif

#define BUFSIZE 32768
#define SIZE_OFFSET 84
#define MD5_DIGEST_LENGTH 16

#define MAX(x, y)  ((x > y) ? x : y)
#define MIN(x, y)  ((x < y) ? x : y)

static int checkmd5sum(int isofd, checkCallback cb, void *cbdata, long long isosize) {
    unsigned char md5[MD5_DIGEST_LENGTH];
    int pagesize = getpagesize();
    unsigned char *buf_unaligned = (unsigned char *) malloc((BUFSIZE + pagesize) * sizeof(unsigned char));
    unsigned char *buf = (buf_unaligned + (pagesize - ((uintptr_t) buf_unaligned % pagesize)));

    // Rewind
    long long offset = lseek64(isofd, 0LL, SEEK_SET);

    // Compute md5
    MD5_CTX md5ctx;
    MD5_Init(&md5ctx);

    if (cb) {
        cb(cbdata, 0, isosize);
    }

    while (offset < isosize) {
        ssize_t nattempt = MIN(isosize - offset, BUFSIZE);

        ssize_t nread = read(isofd, buf, nattempt);
        if (nread <= 0)
            break;

        if (nread > nattempt) {
            nread = nattempt;
            lseek64(isofd, offset + nread, SEEK_SET);
        }

        MD5_Update(&md5ctx, buf, nread);

        offset = offset + nread;
        if (cb && offset / nread % 256 == 0) {
            if (cb(cbdata, offset, isosize)) {
                free(buf_unaligned);
                return ISOMD5SUM_CHECK_ABORTED;
            }
        }
    }

    if (cb) {
        cb(cbdata, isosize, isosize);
    }

    free(buf_unaligned);

    unsigned char computedsum[MD5_DIGEST_LENGTH];
    MD5_Final(computedsum, &md5ctx);

    printf("computedsum=\n");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", computedsum[i]);
    }
    printf("\n");

    printf("md5=\n");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", md5[i]);
    }
    printf("\n");

    if (strncmp((char*)md5, (char*)computedsum, MD5_DIGEST_LENGTH) == 0) {
        return ISOMD5SUM_CHECK_PASSED;
    } else {
        return ISOMD5SUM_CHECK_FAILED;
    }
}

int mediaCheckFile(const char *file, checkCallback cb, void *cbdata) {
    int isofd;

#ifdef _WIN32
    isofd = open(file, O_RDONLY | O_BINARY);
#else
    isofd = open(file, O_RDONLY);
#endif

    if (isofd < 0) {
        return ISOMD5SUM_FILE_NOT_FOUND;
    }

    // Calculate file size
    off_t size = lseek(isofd, 0L, SEEK_END);
    lseek(isofd, 0L, SEEK_SET);

    int rc = checkmd5sum(isofd, cb, cbdata, size);

    close(isofd);

    return rc;
}

int mediaCheckFD(int fd, checkCallback cb, void *cbdata) {
    if (fd < 0) {
        return ISOMD5SUM_FILE_NOT_FOUND;
    }

    // NOTE: files that are FD(written to drive) are implicitly always iso's 
    // Get size
    int pagesize = getpagesize();
    unsigned char *buf_unaligned = (unsigned char *) malloc((BUFSIZE + pagesize) * sizeof(unsigned char));
    unsigned char *buf = (buf_unaligned + (pagesize - ((uintptr_t) buf_unaligned % pagesize)));
    if (lseek64(fd, (16LL * 2048LL), SEEK_SET) == -1) {
        goto fail;
    }

    long long offset = (16LL * 2048LL);
    for (;1;) {
        if (read(fd, buf, 2048) <= 0) {
            goto fail;
        }

        if (buf[0] == 1) {
        /* found primary volume descriptor */
            break;
        } else if (buf[0] == 255) {
        /* hit end and didn't find primary volume descriptor */
            goto fail;
        }
        offset += 2048LL;
    }

    // Get isosize from pvd
    long long isosize = (buf[SIZE_OFFSET] * 0x1000000 + buf[SIZE_OFFSET + 1] * 0x10000 + buf[SIZE_OFFSET + 2] * 0x100 + buf[SIZE_OFFSET + 3]) * 2048LL;

    free(buf_unaligned);

    int rc = checkmd5sum(fd, cb, cbdata, isosize);

    return rc;

fail:
    free(buf_unaligned);
    return ISOMD5SUM_CHECK_NOT_FOUND;
}
