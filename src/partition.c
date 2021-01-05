/*

 partition.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "partition.h"
#include <string.h>
#include <stdlib.h>

static partition_t work_partition;
static char null_path;

typedef struct Partition {
    Petscii name[17];
    char *path;
} Partition;

Partition partitions[256];

void partition_create(partition_t n, const Petscii *name) {
    Partition *p = &partitions[n];
    unsigned int i;

    for (i = 0; name[i] != '\0' && i < sizeof(p->name) - 1; i++) {
        p->name[i] = name[i];
    }
    p->name[i] = 0;
    p->path = &null_path;
}

int partition_select(partition_t n) {
    Partition *p = &partitions[n];

    if (p->path == NULL) return 1;
    work_partition = n;
    return 0;
}

char *partition_get_path(partition_t n) {
    return partitions[n ? n : work_partition].path;
}

partition_t partition_get_current(void) {
    return work_partition;
}

const Petscii *partition_get_name(partition_t n) {
    const Partition *p = &partitions[n ? n : work_partition];

    return (p->path != NULL) ? (Petscii *)p->name : (Petscii *)NULL;
}

void partition_set_path(partition_t n, const char *path) {
    Partition *p;
    p = &partitions[n ? n : work_partition];

    if (p->path != NULL) {
        if (path == NULL || path[0] == 0) {
            if (p->path != &null_path) {
                free(p->path);
                p->path = &null_path;
            }
            return;
        }
        if (strlen(path) > strlen(p->path)) {
            p->path = (char *)realloc(p->path == &null_path ? NULL : p->path, strlen(path) + 1);
            if (p->path == NULL) exit(1); //out of memory?!
        }
        strcpy(p->path, path);
    }
}
