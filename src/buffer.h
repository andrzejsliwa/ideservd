/*

 buffer.h - A mostly working PCLink server for IDEDOS 0.9x

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
#ifndef _BUFFER_H
#define _BUFFER_H
#include <stdio.h>

typedef enum Buffermode {
    CM_CLOSED, CM_DIR, CM_FILE, CM_COMPAT, CM_ERR
} Buffermode;

typedef struct Buffer {
    unsigned char *data;
    size_t pointer, size, capacity;
    FILE *file;
    int fd;
    Buffermode mode;
    unsigned int filesize, filepos;
} Buffer;

struct Directory;
typedef unsigned char Petscii;

extern int buffer_reserve(Buffer *, size_t);
extern int buffer_cookeddir(Buffer *, struct Directory *, int, const Petscii *);
extern int buffer_rawdir(Buffer *, struct Directory *);
extern int buffer_partition(Buffer *);
#endif
