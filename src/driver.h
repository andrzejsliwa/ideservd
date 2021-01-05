/*

 driver.h - A mostly working PCLink server for IDEDOS 0.9x

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
#ifndef _DRIVER_H
#define _DRIVER_H

typedef struct Driver {
    const char *name;
    int  (*initialize)(int);
    int  (*getb)(int);
    void (*sendb)(unsigned char);
    void (*getbytes)(unsigned char [], unsigned int);
    void (*sendbytes)(const unsigned char [], unsigned int);
    void (*shutdown)(void);
    int  (*flush)(void);
    int  (*done)(void);
    void (*turn)(void);
    int  (*wait)(unsigned char);
    int  (*clean)(void);
} Driver;
#endif
