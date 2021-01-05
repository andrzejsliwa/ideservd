/*

 parport.h

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
#ifndef _PARPORT_H
#define _PARPORT_H

extern int parport_init(const char *, int, int);
extern void parport_deinit(void);

#ifdef WIN32
extern int parport_libinit(void);

typedef short _stdcall (*inpfuncPtr)(short portaddr);
typedef void _stdcall (*oupfuncPtr)(short portaddr, short datum);

extern inpfuncPtr inp32;
extern oupfuncPtr oup32;

static __inline__ void outb(unsigned char _data, unsigned short _port)
{
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

static __inline__ unsigned char inb(unsigned short _port)
{
    unsigned char rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

#elif defined __FreeBSD__
static __inline unsigned char inb(unsigned short port) {
    unsigned char value;
    __asm __volatile("inb %%dx,%0" : "=a" (value) : "d" (port));
    return value;
}

static __inline void outb(unsigned char value, unsigned short port) {
    __asm __volatile("outb %0,%1" : : "a" (value), "id" (port));
}
#elif defined __DJGPP__
static inline unsigned char inb(unsigned short _port)
{
    unsigned char rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

static inline void outb(unsigned char _data, unsigned short _port)
{
    __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}
#else
#define PARPORT

#include <sys/io.h>
#endif // WIN32

#ifdef PARPORT
#include <linux/ppdev.h>
#include <linux/parport.h>
#include <sys/ioctl.h>

extern int parportfd;
#endif

#endif
