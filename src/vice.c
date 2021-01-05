/*

 vice.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "vice.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

#include "eth.h"
#include "crc8.h"
#include "log.h"
#include "driver.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 64245

static int sock = -1;
static unsigned char ebufi[1024], ebufo[1024];
static unsigned int ebufip, ebufop, ebufil;
static struct hostent *hp;
static const char *i_addr;
static int i_port;
static int inited;
static int driver_errno;
#ifdef __MINGW32__
static int wsainited;

static const wchar_t *wsaerror(int wsaerrno) {
    static wchar_t msg[100];
    size_t len;
    msg[0] = 0;
    len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, wsaerrno, 0, (LPWSTR)&msg, sizeof msg, NULL);
    if (len > 0 && msg[len - 1] == '\n') msg[--len] = 0;
    if (len > 0 && msg[len - 1] == '\r') msg[--len] = 0;
    if (len > 0 && msg[len - 1] == '.') msg[--len] = 0;
    return msg;
}
#endif

static int flush(void);

static int initialize(int lastfail) {
    struct sockaddr_in eserver;
#ifdef __MINGW32__
    char tr = 1;
    if (wsainited == 0) {
        WORD versionWanted = MAKEWORD(1, 1);
        WSADATA wsaData;
        int wsaerrno = WSAStartup(versionWanted, &wsaData);
        if (wsaerrno != 0) {
            if (lastfail != -9) log_printf("Cannot start winsock: %S(%d)", wsaerror(wsaerrno), wsaerrno);
            return -9;
        }
        wsainited = 1;
    }
#else
    int tr = 1;
#endif

    inited = 0;

    if (!hp) hp = gethostbyname(i_addr);
    if (!hp) {
        if (lastfail != -1) log_printf("Hostname %s not found", i_addr);
        return -1;
    }
    memset(&eserver, 0, sizeof eserver);
    eserver.sin_family = AF_INET;
    memcpy(&eserver.sin_addr.s_addr, hp->h_addr, sizeof eserver.sin_addr.s_addr);
    eserver.sin_port = htons(i_port);

    if (sock < 0) sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        if (lastfail != -2) {
#ifdef __MINGW32__
            int wsaerrno = WSAGetLastError();
            log_printf("Cannot open socket: %S(%d)", wsaerror(wsaerrno), wsaerrno);
#else
            log_printf("Cannot open socket: %s(%d)", strerror(errno), errno);
#endif
        }
        return -2;
    }
    if (connect(sock, (struct sockaddr *)&eserver, sizeof eserver) < 0) {
        if (lastfail != -3) {
#ifdef __MINGW32__
            int wsaerrno = WSAGetLastError();
            log_printf("Cannot connect to %s:%d: %S(%d)", i_addr, i_port, wsaerror(wsaerrno), wsaerrno);
#else
            log_printf("Cannot connect to %s:%d: %s(%d)", i_addr, i_port, strerror(errno), errno);
#endif
        }
        return -3;
    }
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &tr, sizeof tr) < 0) {
#ifdef __MINGW32__
        int wsaerrno = WSAGetLastError();
        log_printf("Setting socket options failed: %S(%d)", wsaerror(wsaerrno), wsaerrno);
#else
        log_printf("Setting socket options failed: %s(%d)", strerror(errno), errno);
#endif
    }

    log_printf("Connected to %s:%d", i_addr, i_port);
    inited = 1;
    return 0;
}

static int refill(void) {
    if (driver_errno == 0) {
        int n = recv(sock, (char *)ebufi, sizeof ebufi, MSG_NOSIGNAL);
        ebufip = ebufil = 0;
        if (n <= 0) return -EIO;
        ebufil = n;
    }
    return driver_errno;
}

static int getb(int use_timeout) {
    (void)use_timeout;
    unsigned char a;
    if (ebufip >= sizeof ebufi || ebufip >= ebufil) {
        driver_errno = refill();
        if (driver_errno != 0) return EOF;
    }
    a = ebufi[ebufip++];
    crc_add_byte(a);
    return a;
}

static void sendb(unsigned char a) {
    if (ebufop >= sizeof ebufo) {
        driver_errno = flush();
        if (driver_errno != 0) return;
    }
    crc_add_byte(a);
    ebufo[ebufop++] = a;
}

static void getbytes(unsigned char data[], unsigned int bytes) {
    while (ebufip + bytes > ebufil) {
        ebufil -= ebufip;
        memcpy(data, ebufi + ebufip, ebufil);
        crc_add_block(data, ebufil);
        data += ebufil; bytes -= ebufil;
        driver_errno = refill();
        if (driver_errno != 0) return;
    }
    memcpy(data, ebufi + ebufip, bytes);
    crc_add_block(data, bytes);
    ebufip += bytes;
}

static void sendbytes(const unsigned char data[], unsigned int bytes) {
    crc_add_block(data, bytes);
    while (ebufop + bytes > sizeof ebufo) {
        memcpy(ebufo + ebufop, data, sizeof(ebufo) - ebufop);
        data += sizeof(ebufo) - ebufop;
        bytes -= sizeof(ebufo) - ebufop;
        ebufop = sizeof ebufo;
        driver_errno = flush();
        if (driver_errno != 0) return;
    }
    memcpy(ebufo + ebufop, data, bytes);
    ebufop += bytes;
}

static void eshutdown(void) {
    if (sock >= 0) close(sock);
    sock = -1;
    inited = 0;

#ifdef __MINGW32__
    if (wsainited == 1) {
        WSACleanup();
        wsainited = 0;
    }
#endif
    return;
}

static int flush(void) {
    if (ebufop && driver_errno == 0) {
        if (send(sock, (char *)ebufo, ebufop, MSG_NOSIGNAL) < 0) {
            ebufop = 0;
            return -EIO;
        }
        ebufop = 0;
    }
    return driver_errno;
}

static int done(void) {
    return driver_errno;
}

static void turn(void) {
}

static int clean(void) {
    ebufip = 0;
    ebufop = 0;
    ebufil = 0;
    return 0;
}

static int waitb(unsigned char ec) {
    int i;
    (void)ec;
    if (sock < 0 || !inited) return -ENODEV;
    driver_errno = 0;
    i = getb(1);
    return (i != EOF) ? i : driver_errno;
}

static const Driver driver = {
    .name         = "VICE",
    .initialize   = initialize,
    .getb         = getb,
    .sendb        = sendb,
    .getbytes     = getbytes,
    .sendbytes    = sendbytes,
    .shutdown     = eshutdown,
    .flush        = flush,
    .done         = done,
    .turn         = turn,
    .wait         = waitb,
    .clean        = clean,
};

const Driver *vice_driver(const char *addr, int port) {
    i_addr = addr != NULL ? addr : SERVER_IP;
    i_port = port != 0 ? port : SERVER_PORT;
    log_printf("Using %s driver connecting to %s:%d", driver.name, i_addr, i_port);
    return &driver;
}
