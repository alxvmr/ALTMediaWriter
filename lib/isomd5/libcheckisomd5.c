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

#define APPDATA_OFFSET 883
#define SIZE_OFFSET 84
#define MD5_DIGEST_LENGTH 16

/* Length in characters of string used for fragment md5sum checking */
#define FRAGMENT_SUM_LENGTH 60

#define MAX(x, y)  ((x > y) ? x : y)
#define MIN(x, y)  ((x < y) ? x : y)

/* finds primary volume descriptor and returns info from it */
/* mediasum must be a preallocated buffer at least 33 bytes long */
/* fragmentsums must be a preallocated buffer at least FRAGMENT_SUM_LENGTH+1 bytes long */
static int parsepvd(int isofd, char *mediasum, int *skipsectors, long long *isosize, int *supported, char *fragmentsums, long long *fragmentcount) {
    int pagesize = getpagesize();
    unsigned char *buf_unaligned = (unsigned char *) malloc((2048LL + pagesize) * sizeof(unsigned char));
    unsigned char *buf = (buf_unaligned + (pagesize - ((uintptr_t) buf_unaligned % pagesize)));
    char buf2[512];
    char tmpbuf[512];
    int skipfnd, md5fnd, supportedfnd, fragsumfnd, fragcntfnd;
    unsigned int loc;
    long long offset;
    char *p;

    if (lseek64(isofd, (16LL * 2048LL), SEEK_SET) == -1)
        goto fail;

    offset = (16LL * 2048LL);
    for (;1;) {
        if (read(isofd, buf, 2048) <= 0)
            goto fail;

        if (buf[0] == 1)
            /* found primary volume descriptor */
            break;
        else if (buf[0] == 255)
            /* hit end and didn't find primary volume descriptor */
            goto fail;
        offset += 2048LL;
    }

    /* read out md5sum */
    memcpy(buf2, buf + APPDATA_OFFSET, 512);
    buf2[511] = '\0';

    *supported = 0;

    md5fnd = 0;
    skipfnd = 0;
    fragsumfnd = 0;
    fragcntfnd = 0;
    supportedfnd = 0;
    loc = 0;
    while (loc < 512) {
        if (!strncmp(buf2 + loc, "ISO MD5SUM = ", 13)) {

            /* make sure we dont walk off end */
            if ((loc + 32 + 13) > 511)
                goto fail;

            memcpy(mediasum, buf2 + loc + 13, 32);
            mediasum[32] = '\0';
            md5fnd = 1;
            loc += 45;
            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else if (!strncmp(buf2 + loc, "SKIPSECTORS = ", 14)) {
            char *errptr;

            /* make sure we dont walk off end */
            if ((loc + 14) > 511)
                goto fail;

            loc = loc + 14;
            for (p=tmpbuf; loc < 512 && buf2[loc] != ';'; p++, loc++)
                *p = buf2[loc];

            *p = '\0';

            *skipsectors = strtol(tmpbuf, &errptr, 10);
            if (errptr && *errptr) {
                goto fail;
            } else {
                skipfnd = 1;
            }

            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else if (!strncmp(buf2 + loc, "RHLISOSTATUS=1", 14)) {
            *supported = 1;
            supportedfnd = 1;
            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else if (!strncmp(buf2 + loc, "RHLISOSTATUS=0", 14)) {
            *supported = 0;
            supportedfnd = 1;
            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else if (!strncmp(buf2 + loc, "FRAGMENT SUMS = ", 16)) {
            /* make sure we dont walk off end */
            if ((loc + FRAGMENT_SUM_LENGTH) > 511)
                goto fail;

            memcpy(fragmentsums, buf2 + loc + 16, FRAGMENT_SUM_LENGTH);
            fragmentsums[FRAGMENT_SUM_LENGTH] = '\0';
            fragsumfnd = 1;
            loc += FRAGMENT_SUM_LENGTH + 16;
            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else if (!strncmp(buf2 + loc, "FRAGMENT COUNT = ", 17)) {
            char *errptr;
            /* make sure we dont walk off end */
            if ((loc + 17) > 511)
                goto fail;

            loc = loc + 17;
            for (p=tmpbuf; loc < 512 && buf2[loc] != ';'; p++, loc++)
                *p = buf2[loc];

            *p = '\0';

            *fragmentcount = strtol(tmpbuf, &errptr, 10);
            if (errptr && *errptr) {
                goto fail;
            } else {
                fragcntfnd = 1;
            }

            for (p=buf2+loc; loc < 512 && *p != ';'; p++, loc++);
        } else {
            loc++;
        }

        if ((skipfnd & md5fnd & fragsumfnd & fragcntfnd) & supportedfnd)
            break;
    }

    if (!(skipfnd & md5fnd))
        goto fail;

    /* get isosize */
    *isosize = (buf[SIZE_OFFSET]*0x1000000+buf[SIZE_OFFSET+1]*0x10000 +
        buf[SIZE_OFFSET+2]*0x100 + buf[SIZE_OFFSET+3]) * 2048LL;

    free(buf_unaligned);
    return offset;

    fail:
    free(buf_unaligned);
    return -1LL;

}

static int checkmd5sum(int isofd, checkCallback cb, void *cbdata) {
    unsigned char md5[MD5_DIGEST_LENGTH];
    int pagesize = getpagesize();
    unsigned int bufsize = 32768;
    unsigned char *buf_unaligned = (unsigned char *) malloc((bufsize + pagesize) * sizeof(unsigned char));
    unsigned char *buf = (buf_unaligned + (pagesize - ((uintptr_t) buf_unaligned % pagesize)));

    // Look for pvd
    if (lseek64(isofd, (16LL * 2048LL), SEEK_SET) == -1) {
        goto fail;
    }

    long long offset = (16LL * 2048LL);
    for (;1;) {
        if (read(isofd, buf, 2048) <= 0) {
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

    // Rewind
    lseek64(isofd, 0LL, SEEK_SET);

    // Compute md5
    MD5_CTX md5ctx;
    MD5_Init(&md5ctx);

    if (cb) {
        cb(cbdata, 0, isosize);
    }

    while (offset < isosize) {
        ssize_t nattempt = MIN(isosize - offset, bufsize);

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

fail:
    free(buf_unaligned);
    return ISOMD5SUM_CHECK_NOT_FOUND;
}

static int doMediaCheck(int isofd, checkCallback cb, void *cbdata) {
    int rc = checkmd5sum(isofd, cb, cbdata);

    return rc;
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


    int rc = doMediaCheck(isofd, cb, cbdata);

    close(isofd);

    return rc;
}

int mediaCheckFD(int fd, checkCallback cb, void *cbdata) {
    int rc;
    if (fd < 0) {
        return ISOMD5SUM_FILE_NOT_FOUND;
    }

    rc = doMediaCheck(fd, cb, cbdata);

    return rc;
}

int printMD5SUM(char *file) {
    int isofd;
    char mediasum[64];
    long long isosize;
    char fragmentsums[FRAGMENT_SUM_LENGTH+1];
    long long fragmentcount = 0;
    int supported;
    int skipsectors;

    isofd = open(file, O_RDONLY);

    if (isofd < 0) {
        return ISOMD5SUM_FILE_NOT_FOUND;
    }

    if (parsepvd(isofd, mediasum, &skipsectors, &isosize, &supported, fragmentsums, &fragmentcount) < 0) {
        return ISOMD5SUM_CHECK_NOT_FOUND;
    }

    close(isofd);

    printf("%s:   %s\n", file, mediasum);
    if ( (strlen(fragmentsums) > 0) && (fragmentcount > 0) ) {
        printf("Fragment sums: %s\n", fragmentsums);
#ifdef _WIN32
        printf("Fragment count: %"PRId64"\n", fragmentcount);
#else
        printf("Fragment count: %lld\n", fragmentcount);
#endif
        printf("Supported ISO: %s\n", supported ? "yes" : "no");
    }

    return 0;
}
