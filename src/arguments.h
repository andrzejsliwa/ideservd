/*

 arguments.h - A mostly working PCLink server for IDEDOS 0.9x

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
#ifndef _ARGUMENTS_H
#define _ARGUMENTS_H
#include "nameconversion.h"

#define VERSION2 "0.32A"
#define VERSION VERSION2 " 20201219"

enum e_modes {
    M_NONE, M_X1541, M_XE1541, M_XM1541, M_XA1541, M_PC64, M_PC64S, M_RS232,
    M_RS232S, M_USB, M_VICE, M_ETHERNET
};

typedef struct Arguments {
    int background;
    int priority;
    Nameconversion nameconversion;
    int debug, verbose;
    const char *user;
    const char *group;
    char *root;
    char *log;
    int hog;
    const char *device;
    unsigned short int lptport;
    char *mode_name;
    enum e_modes mode;
    char *sin_addr;
    unsigned char network;
} Arguments;

extern void testarg(Arguments *, int, char *[]);

#endif
