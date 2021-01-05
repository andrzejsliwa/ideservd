/*

 log.h

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
#ifndef _LOG_H
#define _LOG_H

#ifdef __GNUC__
#define LOG_ATTRIBUTE_PRINTF __attribute__((format (printf, 1, 2)))
#endif

extern void logwindow_set(void *);
extern void log_open(const char *);
extern void log_print(const char *);
extern void log_printf(const char *, ...) LOG_ATTRIBUTE_PRINTF;
extern void log_time(const char *, unsigned long long);
extern void log_speed(const char *, unsigned int, unsigned long long);
extern void log_hex(const unsigned char *);
extern void log_flush(void);

#endif
