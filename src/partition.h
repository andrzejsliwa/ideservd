/*

 partition.h - A mostly working PCLink server for IDEDOS 0.9x

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
#ifndef _PARTITION_H
#define _PARTITION_H

typedef unsigned char partition_t;
typedef unsigned char Petscii;

extern void partition_create(partition_t, const Petscii *);
extern int partition_select(partition_t);
extern char *partition_get_path(partition_t);
extern const Petscii *partition_get_name(partition_t);
extern partition_t partition_get_current(void);
extern void partition_set_path(partition_t, const char *);
#endif
