/*

 path.c - A mostly working PCLink server for IDEDOS 0.9x

 Written by
  Kajtar Zsolt <soci@c64.rulez.org>

    This file is part of IDEDOS the IDE64 disk operating system
    See README for copyright notice.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "path.h"
#ifdef __MINGW32__
#define lstat stat
#define S_ISLNK(a) 0
#define gid_t short
#define uid_t short
#endif
#include "partition.h"
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include "wchar.h"
#include <unistd.h>
#include <errno.h>
#include "avl.h"
#include "shorten.h"
#include "log.h"
#include "ideservd.h"

static const unsigned char latinconv1[] = {
    0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xc1, 0xa4, 0xc3, 0xc5, 0xc5, 0xc5, 0xc5, 0xc9,
    0xc9, 0xc9, 0xc9, 0xa4, 0xce, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xa4, 0xa4, 0xd5,
    0xd5, 0xd5, 0xd5, 0xd9, 0xa4, 0xa4, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0xa4,
    0x43, 0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49, 0xa4, 0x4e, 0x4f, 0x4f,
    0x4f, 0x4f, 0x4f, 0xa4, 0xa4, 0x55, 0x55, 0x55, 0x55, 0x59, 0xa4, 0x59, 0xc1,
    0x41, 0xc1, 0x41, 0xc1, 0x41, 0xc3, 0x43, 0xc3, 0x43, 0xc3, 0x43, 0xc3, 0x43,
    0xc4, 0x44, 0xa4, 0xa4, 0xc5, 0x45, 0xc5, 0x45, 0xc5, 0x45, 0xc5, 0x45, 0xc5,
    0x45, 0xc7, 0x47, 0xc7, 0x47, 0xc7, 0x47, 0xc7, 0x47, 0xc8, 0x48, 0xa4, 0xa4,
    0xc9, 0x49, 0xc9, 0x49, 0xc9, 0x49, 0xc9, 0x49, 0xc9, 0xa4, 0xa4, 0xa4, 0xca,
    0x4a, 0xcb, 0x4b, 0xa4, 0xcc, 0x4c, 0xcc, 0x4c, 0xcc, 0x4c, 0xa4, 0xa4, 0xa4,
    0xa4, 0xce, 0x4e, 0xce, 0x4e, 0xce, 0x4e, 0xa4, 0xa4, 0xa4, 0xcf, 0x4f, 0xcf,
    0x4f, 0xcf, 0x4f, 0xa4, 0xa4, 0xd2, 0x52, 0xd2, 0x52, 0xd2, 0x52, 0xd3, 0x53,
    0xd3, 0x53, 0xd3, 0x53, 0xd3, 0x53, 0xd4, 0x54, 0xd4, 0x54, 0xa4, 0xa4, 0xd5,
    0x55, 0xd5, 0x55, 0xd5, 0x55, 0xd5, 0x55, 0xd5, 0x55, 0xd5, 0x55, 0xd7, 0x57,
    0xd9, 0x59, 0xd9, 0xda, 0x5a, 0xda, 0x5a, 0xda, 0x5a, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xcf, 0x4f, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xd5, 0x55, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xc1, 0x41, 0xc9, 0x49,
    0xcf, 0x4f, 0xd5, 0x55, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xc7, 0x47, 0xcb, 0x4b, 0xcf,
    0x4f, 0xa4, 0xa4, 0xa4, 0xa4, 0x4a, 0xa4, 0xa4, 0xa4, 0xc7, 0x47, 0xa4, 0xa4,
    0xce, 0x4e, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xc1, 0x41, 0xc1, 0x41, 0xc5,
    0x45, 0xc5, 0x45, 0xc9, 0x49, 0xc9, 0x49, 0xcf, 0x4f, 0xcf, 0x4f, 0xd2, 0x52,
    0xd2, 0x52, 0xd5, 0x55, 0xd5, 0x55, 0xd3, 0x53, 0xd4, 0x54, 0xa4, 0xa4, 0xc8,
    0x48, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xc1, 0x41, 0xc5, 0x45, 0xa4, 0xa4,
    0xa4, 0xa4, 0xcf, 0x4f, 0xa4, 0xa4, 0xd9, 0x59
};

static const unsigned char latinconv2[] = {
    0xc1, 0x41, 0xc2, 0x42, 0xc2, 0x42, 0xc2, 0x42, 0xa4, 0xa4, 0xc4, 0x44, 0xc4,
    0x44, 0xc4, 0x44, 0xc4, 0x44, 0xc4, 0x44, 0xa4, 0xa4, 0xa4, 0xa4, 0xc5, 0x45,
    0xc5, 0x45, 0xa4, 0xa4, 0xc6, 0x46, 0xc7, 0x47, 0xc8, 0x48, 0xc8, 0x48, 0xc8,
    0x48, 0xc8, 0x48, 0xc8, 0x48, 0xc9, 0x49, 0xa4, 0xa4, 0xcb, 0x4b, 0xcb, 0x4b,
    0xcb, 0x4b, 0xcc, 0x4c, 0xa4, 0xa4, 0xcc, 0x4c, 0xcc, 0x4c, 0xcd, 0x4d, 0xcd,
    0x4d, 0xcd, 0x4d, 0xce, 0x4e, 0xce, 0x4e, 0xce, 0x4e, 0xce, 0x4e, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xd0, 0x50, 0xd0, 0x50, 0xd2, 0x52, 0xd2,
    0x52, 0xa4, 0xa4, 0xd2, 0x52, 0xd3, 0x53, 0xd3, 0x53, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xd4, 0x54, 0xd4, 0x54, 0xd4, 0x54, 0xd4, 0x54, 0xd5, 0x55, 0xd5,
    0x55, 0xd5, 0x55, 0xa4, 0xa4, 0xa4, 0xa4, 0xd6, 0x56, 0xd6, 0x56, 0xd7, 0x57,
    0xd7, 0x57, 0xd7, 0x57, 0xd7, 0x57, 0xd7, 0x57, 0xd8, 0x58, 0xd8, 0x58, 0xd9,
    0x59, 0xda, 0x5a, 0xda, 0x5a, 0xda, 0x5a, 0x48, 0x54, 0x57, 0x59, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xc1, 0x41, 0xc1, 0x41, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xc5, 0x45, 0xc5, 0x45, 0xc5, 0x45, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xc9, 0x49, 0xc9, 0x49, 0xcf, 0x4f, 0xcf, 0x4f,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xd5, 0x55, 0xd5, 0x55, 0xa4, 0xa4,
    0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xa4, 0xd9, 0x59, 0xd9, 0x59, 0xd9,
    0x59, 0xd9, 0x59
};

static ssize_t topetscii(Petscii *d, const char *s, mbstate_t *ps) {
    unsigned int w;
    Petscii c;
    ssize_t l;
    if ((unsigned char)*s <= '~') {
#if defined WIN32 || defined __DJGPP__
        w = *s; l = 1;
#else
        if (*s == '\\' && (s[1] == 'u' || s[1] == 'U')) {
            int j;
            w = 0; l = 2;
            for (j = (s[1] == 'u') ? 12 : 28; j >= 0; j -= 4, l++) {
                if (s[l] >= '0' && s[l] <= '9') w |= (s[l] - '0') << j;
                else if (s[l] >= 'a' && s[l] <= 'f') w |= (s[l] - 'a' + 10) << j;
                else {
                    w = '\\'; l = 1;
                    break;
                }
            }
        } else {
            w = *s; l = 1;
        }
#endif
    } else {
#if WCHAR_MAX <= 0xFFFFu
        wchar_t w2, w3;
        l = mbrtowc(&w2, s, MB_CUR_MAX, ps);
        if (l < 0) return l;
        if ((w2 ^ 0xd800) < 0x400) {
            ssize_t l2 = mbrtowc(&w3, s + l, MB_CUR_MAX, ps);
            if (l2 < 0) return l2;
            if ((w3 ^ 0xdc00) >= 0x400) {
                return -1;
            }
            w = ((w2 & 0x3ff) << 10) + (w3 & 0x3ff) + 0x10000;
            l += l2;
        } else w = w2;
#else
        wchar_t w2;
        l = mbrtowc(&w2, s, MB_CUR_MAX, ps);
        if (l < 0) return l;
        w = w2;
#endif
    }
    if (w >= 0x20 && w <= 0x40) c = w;
    else if (w >= 0x41 && w <= 0x5a) c = w + 0x80;
    else if (w >= 0x61 && w <= 0x7a) c = w - 0x20;
    else if (w >= 0xe100 && w <= 0xe1ff) c = w - 0xe100;
    else switch (w) {
    case 0x2215: c = 0x2f; break;
    case 0x5b:   c = 0x5b; break;
    case 0xa3:   c = 0x5c; break;
    case 0x5d:   c = 0x5d; break;
    case 0x2191: c = 0x5e; break;
    case 0x2190: c = 0x5f; break;
    case 0xa0:   c = 0xa0; break;
    case 0x258c: c = 0xa1; break;
    case 0x2584: c = 0xa2; break;
    case 0x2594: c = 0xa3; break;
    case 0x5f:   c = 0xa4; break;
    case 0x258f: c = 0xa5; break;
    case 0x2592: c = 0xa6; break;
    case 0x2595: c = 0xa7; break;
    case 0x1fb8f:c = 0xa8; break;
    case 0x1fb99: c = 0xa9; break;
    case 0x1fb87:c = 0xaa; break;
    case 0x251c: c = 0xab; break;
    case 0x2597: c = 0xac; break;
    case 0x2514: c = 0xad; break;
    case 0x2510: c = 0xae; break;
    case 0x2582: c = 0xaf; break;
    case 0x250c: c = 0xb0; break;
    case 0x2534: c = 0xb1; break;
    case 0x252c: c = 0xb2; break;
    case 0x2524: c = 0xb3; break;
    case 0x258e: c = 0xb4; break;
    case 0x258d: c = 0xb5; break;
    case 0x1fb88:c = 0xb6; break;
    case 0x1fb82:c = 0xb7; break;
    case 0x1fb83:c = 0xb8; break;
    case 0x2583: c = 0xb9; break;
    case 0x2713: c = 0xba; break;
    case 0x2596: c = 0xbb; break;
    case 0x259d: c = 0xbc; break;
    case 0x2518: c = 0xbd; break;
    case 0x2598: c = 0xbe; break;
    case 0x259a: c = 0xbf; break;
    case 0x2500: c = 0xc0; break;
    case 0x253c: c = 0xdb; break;
    case 0x1fb8c:c = 0xdc; break;
    case 0x2502: c = 0xdd; break;
    case 0x1fb98:c = 0xdf; break;
    case 0x03c0: c = 0xff; break;
    default: 
        if (w >= 0xc0 && w < 0xc0 + sizeof latinconv1) {
            c = latinconv1[w - 0xc0];
            break;
        }
        if (w >= 0x1e00 && w < 0x1e00 + sizeof latinconv2) {
            c = latinconv2[w - 0x1e00];
            break;
        }
        c = 0xa4; break;
    }
    *d = c;
    return l;
}

size_t c64toascii(char *d, Petscii c, mbstate_t *ps) {
    unsigned int w;
    ssize_t l;
    if (c >= 0x20 && c <= 0x40 && c != 0x2f) w = c;
    else if (c >= 0x41 && c <= 0x5a) w = c + 0x20;
    else if (c >= 0xc1 && c <= 0xda) w = c - 0x80;
    else switch (c) {
    case 0x2f: w = 0x2215; break;
    case 0x5b: w = 0x5b; break;
    case 0x5c: w = 0xa3; break;
    case 0x5d: w = 0x5d; break;
    case 0x5e: w = 0x2191; break;
    case 0x5f: w = 0x2190; break;
    case 0xa0: w = 0xa0; break;
    case 0xa1: w = 0x258c; break;
    case 0xa2: w = 0x2584; break;
    case 0xa3: w = 0x2594; break;
    case 0xa4: w = 0x5f; break;
    case 0xa5: w = 0x258f; break;
    case 0xa6: w = 0x2592; break;
    case 0xa7: w = 0x2595; break;
    case 0xa8: w = 0x1fb8f; break;
    case 0xa9: w = 0x1fb99; break;
    case 0xaa: w = 0x1fb87; break;
    case 0xab: w = 0x251c; break;
    case 0xac: w = 0x2597; break;
    case 0xad: w = 0x2514; break;
    case 0xae: w = 0x2510; break;
    case 0xaf: w = 0x2582; break;
    case 0xb0: w = 0x250c; break;
    case 0xb1: w = 0x2534; break;
    case 0xb2: w = 0x252c; break;
    case 0xb3: w = 0x2524; break;
    case 0xb4: w = 0x258e; break;
    case 0xb5: w = 0x258d; break;
    case 0xb6: w = 0x1fb88; break;
    case 0xb7: w = 0x1fb82; break;
    case 0xb8: w = 0x1fb83; break;
    case 0xb9: w = 0x2583; break;
    case 0xba: w = 0x2713; break;
    case 0xbb: w = 0x2596; break;
    case 0xbc: w = 0x259d; break;
    case 0xbd: w = 0x2518; break;
    case 0xbe: w = 0x2598; break;
    case 0xbf: w = 0x259a; break;
    case 0xc0: w = 0x2500; break;
    case 0xdb: w = 0x253c; break;
    case 0xdc: w = 0x1fb8c; break;
    case 0xdd: w = 0x2502; break;
    case 0xdf: w = 0x1fb98; break;
    case 0xff: w = 0x03c0; break;
    default: w = 0xe100 + c; break;
    }
#if WCHAR_MAX <= 0xFFFFu
    if (w >= 0x10000) {
        l = wcrtomb(d, (wchar_t)(0xd800 + ((w - 0x10000) >> 10)), ps);
        if (l >= 0) {
            ssize_t l2 = wcrtomb(d + l, (wchar_t)(0xdc00 + (w & 0x3ff)), ps);
            l = l2 < 0 ? l2 : (l + l2);
        }
    } else
#endif
    if (w > '~') {
        l = wcrtomb(d, (wchar_t)w, ps);
    } else {
        l = 1; *d = w;
    }
#if defined WIN32 || defined __DJGPP__
    return (l >= 0) ? l : sprintf(d, "?");
#else
    return (l >= 0) ? l : sprintf(d, w <= 0xffff ? "\\u%04x" : "\\U%08x", (unsigned int)w);
#endif
}

typedef struct Filename {
    Petscii name[17];
    Petscii type[4];
    struct avltree_node node;
} Filename;

typedef struct Accesscache {
    dev_t st_dev;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    int result;
} Accesscache;

struct Directory {
    DIR *dir;
    char path[2020];
    char *filename;
    Nameconversion nameconversion;
    int mode;
    struct avltree filenames;
    Accesscache readable, writable, executable;
};

static Filename *lastfn;

static int filename_compare(const struct avltree_node *aa, const struct avltree_node *bb) {
    const Filename *a = cavltree_container_of(aa, Filename, node);
    const Filename *b = cavltree_container_of(bb, Filename, node);
    int i = strcmp((char *)&a->name, (char *)&b->name);
    if (i) return i;
    return strcmp((char *)&a->type, (char *)&b->type);
}

static void filename_free(struct avltree_node *aa) {
    Filename *a = avltree_container_of(aa, Filename, node);
    free(a);
}

Directory *directory_open(const char *path, Nameconversion nameconversion, int mode) {
    size_t len;
    DIR *dir;
    Directory *directory = (Directory *)malloc(sizeof *directory);
    if (directory == NULL) {
        return NULL;
    }
    len = strlen(path);
    dir = opendir(len != 0 ? path : ".");
    if (dir == NULL) {
        free(directory);
        return NULL;
    }
    if (len != 0) memcpy(directory->path, path, len);
    directory->filename = directory->path + len;
    directory->nameconversion = nameconversion;
    directory->mode = mode;
    avltree_init(&directory->filenames);
    directory->dir = dir;
    directory->readable.st_mode = -1;
    directory->writable.st_mode = -1;
    directory->executable.st_mode = -1;
    return directory;
}

int directory_close(Directory *directory) {
    int ret;
    avltree_destroy(&directory->filenames, filename_free);
    free(lastfn);
    lastfn = NULL;
    ret = closedir(directory->dir);
    free(directory);
    return ret;
}

static int cachecheck(const struct stat *buf, Accesscache *cache, mode_t mode) {
    if ((buf->st_mode & mode) != cache->st_mode || buf->st_dev != cache->st_dev || buf->st_uid != cache->st_uid || buf->st_gid != cache->st_gid) {
        cache->st_mode = buf->st_mode & mode;
        cache->st_dev = buf->st_dev;
        cache->st_uid = buf->st_uid;
        cache->st_gid = buf->st_gid;
        return 1;
    }
    return 0;
}

int directory_read(Directory *directory, Directory_entry *kesz) {
    const struct dirent *ep;
    struct avltree_node *b;
    struct stat buf;
    int stated;

    while ((ep = readdir(directory->dir))) {
        const char *filename = ep->d_name;
        char *dst;
        size_t fnlen;

        if (filename[0] == '.' && (filename[1] == 0 || (filename[1] == '.' && filename[2] == 0))) continue;

#ifdef __MINGW32__
        buf.st_mode = 0;
#else
        buf.st_mode = (ep->d_type == DT_UNKNOWN) ? 0 : DTTOIF(ep->d_type);
#endif

        fnlen = strlen(filename);
        if (fnlen > 999) continue;

        dst = directory->filename;
        if (directory->path != dst) *dst++ = '/';
        memcpy(dst, filename, fnlen + 1);

        if ((directory->mode == 0 && buf.st_mode != 0) || (directory->mode < 3 && S_ISDIR(buf.st_mode))) {
            kesz->size = 0;
            kesz->time = 0;
            stated = 0;
        } else {
            if (lstat(directory->path, &buf)) continue;
            kesz->size = buf.st_size;
            kesz->time = buf.st_mtime;
            stated = 1;
        }

        memset(kesz->name, 0, 17);
        if (directory->nameconversion == NC_IGNOREDOT) {
            memcpy(kesz->filetype, "PRG", 4);
        } else {
            memset(kesz->filetype, 0, 4);
        }
        kesz->attrib = A_DELETEABLE;
        if (directory->mode > 2) {
            if (!stated) {
                if (!access(directory->path, X_OK)) {
                    kesz->attrib |= A_EXECUTEABLE;
                }
                if (!access(directory->path, R_OK)) {
                    kesz->attrib |= A_READABLE;
                }
            } else {
                if (cachecheck(&buf, &directory->executable, S_IXUSR | S_IXGRP | S_IXOTH)) {
                    directory->executable.result = access(directory->path, X_OK);
                }
                if (!directory->executable.result) {
                    kesz->attrib |= A_EXECUTEABLE;
                }
                if (cachecheck(&buf, &directory->readable, S_IRUSR | S_IRGRP | S_IROTH)) {
                    directory->readable.result = access(directory->path, R_OK);
                }
                if (!directory->readable.result) {
                    kesz->attrib |= A_READABLE;
                }
            }
        }
        if (directory->mode > 1) {
            if (!stated) {
                if (!access(directory->path, W_OK)) {
                    kesz->attrib |= A_WRITEABLE;
                }
            } else {
                if (cachecheck(&buf, &directory->writable, S_IWUSR | S_IWGRP | S_IWOTH)) {
                    directory->writable.result = access(directory->path, W_OK);
                }
                if (!directory->writable.result) {
                    kesz->attrib |= A_WRITEABLE;
                }
            }
        }
        {
            Petscii *tmp, *filename2, tmpbuf[32];
            size_t i, j, k, l;
            mbstate_t ps;
            memset(&ps, 0, sizeof ps);

            if (fnlen < sizeof tmpbuf) {
                filename2 = tmpbuf;
            } else {
                filename2 = (Petscii *)malloc((fnlen + 1) * sizeof *filename2);
                if (filename2 == NULL) continue;
            }
            for (i = j = 0; filename[i]; j++) {
                ssize_t ll = topetscii(filename2 + j, filename + i, &ps);
                if (ll < 0) {
                    if (filename2 != tmpbuf) free(filename2);
                    continue;
                }
                i += ll;
            }
            filename2[j] = 0;
            tmp = filename2;

            l = k = j; j = 0;
            for (i = 0; i < k; i++) {
                switch (tmp[i]) {
                case '.':
                    if (S_ISDIR(buf.st_mode) || directory->nameconversion == NC_IGNOREDOT) break;
                    /* fall through */
                case ',':
                    if (i == 0) break;
                    memset(kesz->filetype, 0, 4);
                    l = j = i;
                    /* fall through */
                default:
                    break;
                }
            }
            if (l > 16) {
                tmp = shorten(filename2, l);
                if (!tmp) {
                    if (filename2 != tmpbuf) free(filename2);
                    continue;
                }
                l = 16;
            }

            for (i = 0; i < l; i++) {
                Petscii c = tmp[i];
                if (c == ':' || c == '=' || c == '*' || c == '?' || c == ',') {
                    c = 0xa4;
                }
                kesz->name[i] = c;
            }
            if (tmp != filename2) free(tmp);

            if (j) {
                for (i = j + 1; i < k; i++) {
                    Petscii c = filename2[i];
                    if ((i - j) > 3 || c == 0) {
                        break;
                    }
                    if (c == ':' || c == '=' || c == '*' || c == '?' || c == ',' || c == '<' || c == ' ') {
                        c = 0xa4;
                    }
                    kesz->filetype[i - j - 1] = c;
                }
                if (i - j == 2) {
                    kesz->filetype[1] = kesz->filetype[2] = 0xa0;
                }
            }
            if (filename2 != tmpbuf) free(filename2);
        }

        if (S_ISDIR(buf.st_mode)) {
            memcpy(kesz->filetype, "DIR", 4);
            kesz->attrib |= A_DIR | A_CLOSED;
            kesz->size = 0;
        } else if (S_ISREG(buf.st_mode)) {
            kesz->attrib |= A_NORMAL | A_CLOSED;
        } else if (S_ISLNK(buf.st_mode)) {
            memcpy(kesz->filetype, "LNK", 4);
            kesz->attrib |= A_LNK | A_CLOSED;
        } else {
            memcpy(kesz->filetype, "DEL", 4);
            kesz->attrib = A_DEL | A_CLOSED;
            kesz->size = 0;
        }

        if (lastfn == NULL) {
            lastfn = (Filename *)malloc(sizeof *lastfn);
            if (lastfn == NULL) continue;
        }

        memcpy(lastfn->name, kesz->name, 17);
        memcpy(lastfn->type, kesz->filetype, 4);
        b = avltree_insert(&lastfn->node, &directory->filenames, filename_compare);
        if (b) continue;
        lastfn = NULL;
        return 1;
    }
    kesz->attrib = 0;
    directory->filename[0] = 0;
    return 0;
}

int directory_rawread(Directory *directory, unsigned char *out) {
    struct tm ido2;
    Directory_entry dirent;
    int l = directory_read(directory, &dirent);
    if (l == 0) return 0;
    for (l = 0; dirent.name[l] != 0; l++) out[l] = dirent.name[l];
    for (; l < 16; l++) out[l] = 0;
    out[16] = dirent.size;
    out[17] = dirent.size >> 8;
    out[18] = dirent.size >> 16;
    out[19] = dirent.size >> 24;
    out[20] = out[21] = out[22] = out[23] = 0;
    out[24] = dirent.attrib;
    for (l = 0; dirent.filetype[l] != 0; l++) out[l + 25] = dirent.filetype[l];
    for (; l < 3; l++) out[l + 25] = 0;

    ido2 = *localtime(&dirent.time);
    ido2.tm_mon++;
    out[28] = (ido2.tm_sec % 60) | ((ido2.tm_mon << 4) & 0xc0);
    out[29] = (ido2.tm_min % 60) | (ido2.tm_mon << 6);
    out[30] = (((ido2.tm_year - 80) % 100) & 63) | ((ido2.tm_hour << 3) & 0xc0);
    out[31] = (ido2.tm_mday & 31) | (ido2.tm_hour << 5);
    return 1;
}

int directory_entry_cook(const Directory_entry *dirent, unsigned char *out) {
    unsigned int n;
    unsigned int len = dirent->size >> 8;
    if (dirent->size & 0xff) len++;
    if (len > 65535) len = 65535;

    out[0x00] = out[0x01] = 1;
    out[0x02] = len;
    out[0x03] = len >> 8;
    memset(out + 0x04, 0x20, 28);
    out[31] = 0;
    n = 7;
    if (len > 9) n--;
    if (len > 99) n--;
    if (len > 999) n--;
    out += n;
    out[0] = 0x22;
    for (n = 0; dirent->name[n] != 0; n++) out[n + 1] = dirent->name[n];
    out[n + 1] = 0x22;

    if (!(dirent->attrib & A_CLOSED)) out[18] = '*';

    for (n = 0; dirent->filetype[n] != 0; n++) out[n + 19] = dirent->filetype[n];
    if ((dirent->attrib & (A_DELETEABLE | A_WRITEABLE)) != (A_DELETEABLE | A_WRITEABLE)) out[22] = '<';
    return len;
}

const char *directory_path(const Directory *directory) {
    return directory->path;
}

const char *directory_filename(const Directory *directory) {
    return directory->filename[0] != '/' ? directory->filename : (directory->filename + 1);
}

void convertfilename(const Petscii *s, char typek, Petscii *name, Petscii *type, unsigned char *mode) {
    int b, i, j;
    char type2[4], mode2;
    int comma;

    i = comma = mode2 = 0; j = -1;

    for (b = 0; s[b] != 0; b++) {
        if (s[b] == ':') {
            i = comma = mode2 = 0; j = -1; continue;
        }
        if (s[b] == '/') {
            if (typek == 0) break;
        }

        switch (comma) {
        case 0:
            if (s[b] == typek) {
                comma++; j = 0;
                break;
            }
            if (s[b] == ',' || s[b] == '=') {
                comma = 3;
                break;
            }
            if (i < 16) {
                name[i++] = s[b];
            }
            break;
        case 1:
            if (s[b] == ',') {
                comma++;
                break;
            }
            if (s[b] == ',' || s[b] == '=') {
                comma = 3;
                break;
            }
            if (j < 3) {
                type2[j++] = s[b];
            }
            break;
        case 2:
            if (s[b] == ',' || s[b] == '=') {
                comma = 3;
                break;
            }
            mode2 = s[b];
            comma++; break;
        default:
            break;
        }
    }
    name[i] = 0;
    if (j == 1) {
        if (mode2 == 0) {
            switch (type2[0]) {
            case 'W':
            case 'R':
            case 'A':
            case 'M':
                mode2 = type2[0];
                if (mode2 == 'W' && type != NULL && type[0] == '*') {
                    type2[0] = 'S';
                    break;
                }
                j = -1;
                break;
            }
        }
    }
    if (j >= 0 && type != NULL) {
        if (j == 1) {
            switch (type2[0]) {
            case 'S': strcpy(type2, "SEQ"); j = 3; break;
            case 'P': strcpy(type2, "PRG"); j = 3; break;
            case 'U': strcpy(type2, "USR"); j = 3; break;
            case 'B': strcpy(type2, "DIR"); j = 3; break;
            case 'J': strcpy(type2, "LNK"); j = 3; break;
            default: type2[0] = ':'; break;
            }
        }
        memcpy(type, type2, j);
        type[j] = 0;
    }
    if (mode2 && mode) {
        *mode = mode2;
    }
}

int matchname(Petscii *a, Petscii *b) {
    while (*a == *b || (*b == '?' && *a != 0)) {
        if (*a == 0) {
            return 1;
        }
        a++; b++;
    }

    if (*b == '*') {
        b++;
        if (*b == 0) {
            return 1;
        }
        while (*a != 0) {
            if (matchname(a, b)) {
                return 1;
            }
            a++;
        }
    }

    return 0;
}

const Petscii *resolv_path(const Petscii *s, char *outpath, unsigned char *outpart, Nameconversion nameconversion) {
    Petscii name[17];
    int fel = 0, fel2 = 0;
    unsigned char partition = 0;

    if (strchr((char *)s, ':')) {
        if (s[0] == '@') fel = 1;
        while ((s[fel] ^ 0x30) < 10) partition = partition * 10 + (s[fel++] ^ 0x30);
        while (s[fel] && s[fel] != '/' && s[fel] != ':') {
            fel++;
        }
        fel++;
    }

    if (s[fel] == '/') {
        outpath[0] = 0;
    } else {
        const char *path = partition_get_path(partition);
        if (path == NULL) {
            return NULL;
        }
        strcpy(outpath, path);
    }

    for (;;) {
        Directory_entry direntry;
        Directory *directory;

        if (s[fel] == '/' || s[fel] == ':') {
            fel++; continue;
        }

        fel2 = fel;
        if (s[fel] != 0) {
            while (s[fel2] != 0 && s[fel2] != '/' && s[fel2] != ':') {
                fel2++;
            }
        }
        if (s[fel2] == 0) {
            if (outpart != NULL) *outpart = partition;
            return s + fel;
        }

        if (s[fel] == '.') {
            if (fel2 == fel + 1) {
                fel += 2; continue;
            }
            if (s[fel + 1] == '.' && fel2 == fel + 2) {
                char *o = strrchr(outpath, '/');
                if (o == NULL) o = outpath;
                *o = 0;
                fel += 3; continue;
            }
        }

        directory = directory_open(outpath, nameconversion, 0);
        if (directory == NULL) {
            errtochannel15(1);
            log_printf("Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
            return NULL;
        }
        convertfilename(s + fel, 0, name, NULL, NULL);

        while (directory_read(directory, &direntry))
        {
            if ((direntry.attrib & (A_DIR | A_CLOSED)) != (A_CLOSED | A_DIR)) {
                continue;
            }

            if (!matchname(direntry.name, name)) {
                continue;
            }

            if (strlen(directory_path(directory)) > 999) {
                direntry.attrib = 0;
            } else {
                strcpy(outpath, directory_path(directory));
            }
            break;
        }
        directory_close(directory);
        if (direntry.attrib == 0) return NULL;
        fel = fel2;
    }
}

static int badfilename(const char *name) {
    if (name[0] == '.') {
        if (name[1] == 0) return 1;
        if (name[1] == '.' && name[2] == 0) return 1;
    }
    return 0;
}

void convertc64name(char *lname, const Petscii *name, const Petscii *type, Nameconversion nameconversion) {
    mbstate_t ps;
    int i, f2 = 0;
    int doext;
    unsigned char dot;
    memset(&ps, 0, sizeof ps);
    for (i = 0; name[i] != 0 && i < 16; i++) {
        f2 += c64toascii(lname + f2, name[i], &ps);
    }
    lname[f2] = 0;

    dot = nameconversion == NC_FORCEDOT ? '.' : ',';
    if (nameconversion == NC_IGNOREDOT) {
        doext = memcmp(type, "PRG", 3) != 0;
    } else {
        doext = type[0] != 0 || strchr((lname[0] == '.') ? (lname + 1) : lname, '.') != NULL || badfilename(lname);
    }

    for (;;) {
        int f = f2;
        if (doext) {
            memset(&ps, 0, sizeof ps);
            lname[f2++] = dot;
            for (i = 0; type[i] != 0 && i < 3; i++) {
                if (i == 1 && type[1] == 0xa0 && type[2] == 0xa0) break;
                f2 += c64toascii(lname + f2, type[i], &ps);
            }
            lname[f2] = 0;
        }
        if (!badfilename(lname)) break;
        doext = 1;
        f2 = f;
        dot = ',';
    }
}
