/*

 message.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "message.h"
#include <stdarg.h>
#include <stdio.h>
#ifdef WIN32
#include <unistd.h>
#include <windows.h>
#endif

void message(const char *format, ...) {
    va_list args;
    va_start(args, format);
#ifndef WIN32
    vfprintf(stderr, format, args);
#else
    if (isatty(STDERR_FILENO)) {
        vfprintf(stderr, format, args);
    } else {
        size_t s = vsnprintf(NULL, 0, format, args) + 1;
        char *str = (char *)malloc(s);
        if (str != NULL) {
            vsnprintf(str, s, format, args);
            MessageBox(NULL, str, "IDEservd", MB_OK);
            free(str);
        }
    }
#endif
    va_end(args);
}
