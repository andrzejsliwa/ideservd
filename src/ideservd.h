/*

 ideservd.h - A mostly working PCLink server for IDEDOS 0.9x

 Written by
  Kajtar Zsolt <soci@c64.rulez.org>
 Win32 port by
  Josef Soucek <josef.soucek@ide64.org>
 Serial support by
  Borsanyi Attila <bigfoot@axelero.hu>
 Serial fixes by
  Silver Dream <silverdr@inet.com.pl>

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
#ifndef _IDESERV_H
#define _IDESERV_H

typedef unsigned char Petscii;

enum {
    FLAG_USE_CRC = 1, 
    FLAG_USE_PADDING = 2
};

typedef enum Errorcode {
    ER_OK = 0,
    ER_FILES_SCRATCHED = 1,
    ER_PARTITION_SELECTED = 2,
    ER_READ_ERROR = 23,
    ER_WRITE_ERROR = 25,
    ER_WRITE_PROTECT_ON = 26,
    ER_ACCESS_DENIED = 27,
    ER_UNKNOWN_COMMAND = 31,
    ER_SYNTAX_ERROR = 30,
    ER_INVALID_FILENAME = 33,
    ER_MISSING_FILENAME = 34,
    ER_PATH_NOT_FOUND = 39,
    ER_FRAME_ERROR = 41,
    ER_CRC_ERROR = 42,
    ER_FILE_NOT_FOUND = 62,
    ER_FILE_EXISTS = 63,
    ER_FILE_TYPE_MISMATCH = 64,
    ER_NO_CHANNEL = 70,
    ER_PARTITION_FULL = 72,
    ER_DOS_VERSION = 73,
    ER_SELECTED_PARTITION_ILLEGAL = 77,
    ER_UNKNOWN_ERROR = 80
} Errorcode;

extern Errorcode errtochannel15(int);
extern void seterror(Errorcode, int);
extern void commandchannel(const Petscii *);
#endif
