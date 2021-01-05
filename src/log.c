/*

 log.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>

static HWND logwindow_handle = NULL;
#endif

static FILE *lf = NULL;

static void log_close(void) {
    if (lf != NULL) {
        fclose(lf);
        lf = NULL;
    }
}

#ifdef WIN32
void logwindow_set(void *handle) {
    logwindow_handle = handle;
}

static void logwindow_append(const char *text) {
    char *data;
    size_t len;
    if (logwindow_handle == NULL) return;
    len = strlen(text) + 1;
    data = malloc(len);
    if (data == NULL) return;
    memcpy(data, text, len);
    SendMessage(logwindow_handle, WM_USER + 1, 0, (LPARAM)(LPVOID)data);
}
#endif

void log_open(const char *filename) {
    atexit(log_close);

    if (filename == NULL) {
        lf = NULL;
        return;
    }
    if (filename[0] == '-' && filename[1] == 0) {
        lf = NULL;
        return;
    }
    lf = fopen(filename, "wt");
    if (lf == NULL) {
        log_printf("Cannot open logfile \"%s\"", filename);
    }
}

void log_print(const char *text) {
    FILE *f = lf;
#ifdef WIN32
    if (IsWindow(logwindow_handle)) {
        logwindow_append(text);
    }
#else
    if (f == NULL) f = stdout;
#endif
    if (f != NULL) {
        fputs(text, f);
        putc('\n', f);
    }
}

void log_printf(const char *format, ...) {
    FILE *f = lf;
    va_list args;
    va_start(args, format);
#ifdef WIN32
    if (IsWindow(logwindow_handle)) {
        size_t s = vsnprintf(NULL, 0, format, args) + 1;
        char *str = (char *)malloc(s);
        if (str != NULL) {
            vsnprintf(str, s, format, args);
            logwindow_append(str);
            free(str);
        }
    }
#else
    if (f == NULL) f = stdout;
#endif
    if (f != NULL) {
        vfprintf(f, format, args);
        putc('\n', f);
    }
    va_end(args);
}

void log_time(const char *name, unsigned long long l2) {
    const char *unit;
    int p;
    double f;
    if (l2 > 1000000) {
        l2 = (l2 + 500) / 1000;
        unit = "";
    } else if (l2 > 1000) {
        unit = "milli";
    } else if (l2 != 0) {
        unit = "micro";
    } else {
        return;
    }
    if (l2 > 1000) {
        p = (l2 % 1000) == 0 ? 0 : (l2 % 100) == 0 ? 1 : (l2 % 10) == 0 ? 2 : 3;
        f = l2 / 1000.0;
    } else {
        p = 0;
        f = l2;
    }
    log_printf("%s %.*f %sseconds", name, p, f, unit);
}

void log_speed(const char *name, unsigned int bytes, unsigned long long l2) {
    const char *unit;
    int p;
    double f;
    if (l2 != 0) {
        l2 = bytes * 1000000ull / l2;
    }

    if (l2 > 1000000) {
        l2 = (l2 + 500) / 1000;
        unit = "mega";
    } else if (l2 > 1000) {
        unit = "kilo";
    } else if (l2 != 0) {
        unit = "";
    } else {
        log_printf("%s %u bytes", name, bytes);
        return;
    }
    if (l2 > 1000) {
        p = (l2 % 1000) == 0 ? 0 : (l2 % 100) == 0 ? 1 : (l2 % 10) == 0 ? 2 : 3;
        f = l2 / 1000.0;
    } else {
        p = 0;
        f = l2;
    }
    log_printf("%s %u bytes at %.*f %sbytes/seconds", name, bytes, p, f, unit);
}

static void writehex(char *msg, unsigned char c) {
    const char *hex = "0123456789ABCDEF";
    msg[0] = hex[c >> 4];
    msg[1] = hex[c & 15];
    msg[2] = ' ';
}

void log_hex(const unsigned char *cmd) {
    char msg[16 * 3];
    int i = 0;
    while (cmd[i]) {
        writehex(msg + (i & 15) * 3, cmd[i]);
        i++;
        if (!(i & 15)) {
            msg[16 * 3 - 1] = '\0';
            log_print(msg);
        }
    }
    if (i & 15) {
        msg[(i & 15) * 3 - 1] = '\0';
        log_print(msg);
    }
}

void log_flush(void) {
    fflush(lf != NULL ? lf : stdout);
}
