/*

 buffer.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "buffer.h"
#include <string.h>
#include <stdlib.h>
#include "partition.h"
#include "arguments.h"
#include "path.h"

#ifdef WIN32
#define SYSTEMNAME "WIN32"
#elif defined __DJGPP__
#define SYSTEMNAME "DOS32"
#elif defined __FreeBSD__
#define SYSTEMNAME "FRBSD"
#else
#define SYSTEMNAME "LINUX"
#endif

int buffer_reserve(Buffer *buffer, size_t size) {
    if (buffer->capacity < size) {
        unsigned char *data = (unsigned char *)realloc(buffer->data, size);
        if (data == NULL) return 1; //out of memory?!
        buffer->data = data;
        buffer->capacity = size;
    }
    return 0;
}

static int buffer_append(Buffer *buffer, const unsigned char *data, unsigned int size) {
    size_t new_size = buffer->size + size;
    if (new_size < size) return 1; //overflow
    new_size += 4096;
    if (new_size < 4096) return 1; //overflow
    if (buffer_reserve(buffer, new_size)) return 1; //out of memory?!
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return 0;
}

int buffer_cookeddir(Buffer *buffer, Directory *directory, int partition, const Petscii *outname) {
    Directory_entry dirent;
    unsigned char dirline[32];
    Petscii name[17], type[4] = {'*', 0};
    unsigned int sum = 0;

    if (outname != NULL) convertfilename(outname, '=', name, type, NULL);
    if (name[0] == 0) {
        name[0] = '*';
        name[1] = 0;
    }

    memcpy(&dirline[0x00], "\1\4\1\1\1\0\22\42", 8);
    dirline[0x04] = partition != 0 ? partition : partition_get_current();
    memset(&dirline[0x08], 32, 16);
    memcpy(&dirline[0x18], "\42\40" SYSTEMNAME "\0", 8);
    if (buffer_append(buffer, dirline, 32)) return 1;

    while (directory_read(directory, &dirent))
    {
        if (!matchname(dirent.name, name)) continue;
        if (!matchname(dirent.filetype, type)) continue;

        sum += directory_entry_cook(&dirent, dirline);
        if (sum > 65535) sum = 65535;
        if (buffer_append(buffer, dirline, 32)) return 1;
    }

    dirline[0x00] = dirline[0x01] = 1;
    dirline[0x02] = sum;
    dirline[0x03] = sum >> 8;
    memcpy(&dirline[0x04], "BLOCKS USED.             \0\0\0", 28);
    return buffer_append(buffer, dirline, 32);
}

int buffer_rawdir(Buffer *buffer, Directory *directory) {
    unsigned char dirline[33];

    dirline[0] = 'I';
    memset(dirline + 1, 32, 16);
    memset(dirline + 17, 0, 8);
    dirline[25] = A_DIR;
    memcpy(dirline + 26, "DIR", 3);
    memset(dirline + 29, 0, 4);
    if (buffer_append(buffer, dirline, 33)) return 1;

    while (directory_rawread(directory, dirline))
    {
        if (buffer_append(buffer, dirline, 32)) return 1;
    }
    return 0;
}

int buffer_partition(Buffer *buffer) {
    unsigned char dirline[32];
    int i, n = 0;
    memcpy(&dirline[0x00], "\1\4\1\1\377\0\22\42", 8);
    memcpy(&dirline[0x08], "IDESERVD " VERSION2 "  ", 16);
    memcpy(&dirline[0x18], "\42\40" SYSTEMNAME "\0", 8);
    if (buffer_append(buffer, dirline, 32)) return 1;
    for (i = 1; i < 255; i++) {
        const Petscii *name = partition_get_name(i);
        if (name == NULL) continue;
        dirline[0x00] = dirline[0x01] = 1;
        dirline[0x02] = i;
        dirline[0x03] = 0;
        memcpy(&dirline[0x04], "   \42\42                 NET  \0", 28);
        memcpy(&dirline[0x08], name, strlen((char *)name));
        dirline[0x08 + strlen((char *)name)] = '"';
        if (buffer_append(buffer, dirline, 32)) return 1;
        n++;
    }
    dirline[0x00] = dirline[0x01] = 1;
    dirline[0x02] = n;
    dirline[0x03] = 0;
    memcpy(&dirline[0x04], "PARTITIONS.              \0\0\0", 28);
    return buffer_append(buffer, dirline, 32);
}

