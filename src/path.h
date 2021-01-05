/*

 path.h - A mostly working PCLink server for IDEDOS 0.9x

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
#ifndef _PATH_H
#define _PATH_H
#include <wchar.h>
#include <time.h>
#include "nameconversion.h"

typedef unsigned char Petscii;

typedef struct Directory_entry {
    Petscii filetype[4];
    Petscii name[17];
    unsigned char attrib;
    unsigned int size;
    time_t time;
} Directory_entry;

enum {
    A_CLOSED      = 0x80,
    A_DELETEABLE  = 0x40,
    A_READABLE    = 0x20,
    A_WRITEABLE   = 0x10,
    A_EXECUTEABLE = 0x08,
    A_DEL         = 0x00,
    A_NORMAL      = 0x01,
    A_REL         = 0x02,
    A_DIR         = 0x03,
    A_LNK         = 0x04,
    A_ANY         = 0x07
};

struct dirent;
typedef struct Directory Directory;

extern Directory *directory_open(const char *, Nameconversion, int);
extern int directory_read(Directory *, Directory_entry *);
extern int directory_entry_cook(const Directory_entry *, unsigned char *);
extern int directory_rawread(Directory *, unsigned char *);
extern const char *directory_path(const Directory *);
extern const char *directory_filename(const Directory *);
extern int directory_close(Directory *);
extern void convertfilename(const Petscii *, char, Petscii *, Petscii *, unsigned char *);
extern int matchname(Petscii *, Petscii *);
extern const Petscii *resolv_path(const Petscii *, char *, unsigned char *, Nameconversion);
extern size_t c64toascii(char *, Petscii, mbstate_t *);
extern void convertc64name(char *, const Petscii *, const Petscii *, Nameconversion);
#endif
