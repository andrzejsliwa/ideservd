/*

 crc8.h

 Written by
  Kajtar Zsolt <soci@c64.rulez.org>
 Win32 port by
  Josef Soucek <josef.soucek@ide64.org>

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
#ifndef _CRC8_H
#define _CRC8_H

extern void crc_clear(int);
extern void crc_add_byte(unsigned char);
extern void crc_add_block(const unsigned char [], unsigned int);
extern int crc_get(void);
#endif
