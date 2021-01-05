/*

 eth.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "eth.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef __MINGW32__
#include <winsock2.h>
#define socklen_t ssize_t
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include "crc8.h"
#include "log.h"
#include "driver.h"

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define CLIENT_IP "127.0.0.1"
#define SERVER_IP INADDR_ANY
#define SERVER_PORT 64000

static int sock = -1;
static unsigned char ebufi[1030], ebufo[1030];
static unsigned int ebufip, ebufop, ebufil;
static struct sockaddr_in eserver;
static struct sockaddr_in eclient;
static socklen_t fromlen = sizeof(struct sockaddr_in);
static struct in_addr sin_addr;
static struct hostent *hp;
static const char *i_addr;
static int i_network;
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

static int initialize(int lastfail) {
    inited = 0;

#ifdef __MINGW32__
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
#endif

    if (!hp) hp = gethostbyname(i_addr);
    if (!hp) {
        if (lastfail != -1) log_printf("Hostname \"%s\" not found", i_addr);
        return -1;
    }
    memcpy(&sin_addr, hp->h_addr, sizeof sin_addr);

    if (sock < 0) sock = socket(AF_INET, SOCK_DGRAM, 0);
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
    memset(&eserver, 0, sizeof eserver);
    eserver.sin_family = AF_INET;
    eserver.sin_addr.s_addr = SERVER_IP;
    eserver.sin_port = htons(SERVER_PORT + i_network);
    if (bind(sock, (struct sockaddr *)&eserver, sizeof eserver) < 0) {
        if (lastfail != -3) {
#ifdef __MINGW32__
            int wsaerrno = WSAGetLastError();
            log_printf("Cannot bind to port %d: %S(%d)", SERVER_PORT + i_network, wsaerror(wsaerrno), wsaerrno);
#else
            log_printf("Cannot bind to port %d: %s(%d)", SERVER_PORT + i_network, strerror(errno), errno);
#endif
        }
        return -3;
    }

    log_printf("Listening on port %d for C64 at %s", SERVER_PORT + i_network, i_addr);
    inited = 1;
    return 0;
}

static int getb(int use_timeout) {
    (void)use_timeout;
    if (driver_errno != 0) return EOF;
    if (ebufip >= sizeof ebufi) return EOF;
    if (ebufip >= ebufil) return EOF;
    return ebufi[ebufip++];
}

static void sendb(unsigned char a) {
    if (ebufop >= sizeof ebufo) {
        if (driver_errno != 0) return;
        driver_errno = -EIO;
        return;
    }
    ebufo[ebufop++] = a;
}

static void getbytes(unsigned char data[], unsigned int bytes) {
    if (ebufip + bytes > ebufil) {
        memcpy(data, ebufi + ebufip, ebufil - ebufip);
        ebufip = ebufil;
        if (driver_errno != 0) return;
        driver_errno = -EIO;
        return;
    }
    memcpy(data, ebufi + ebufip, bytes);
    ebufip += bytes;
}

static void sendbytes(const unsigned char data[], unsigned int bytes) {
    if (ebufop + bytes > sizeof ebufo) {
        memcpy(ebufo + ebufop, data, sizeof(ebufo) - ebufop);
        ebufop = sizeof ebufo;
        if (driver_errno != 0) return;
        driver_errno = -EIO;
        return;
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
    if (ebufop != 0 && driver_errno == 0) {
        if (sendto(sock, (char *)ebufo, ebufop, MSG_NOSIGNAL, (struct sockaddr *)&eclient, fromlen) < 0) {
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
    ebufil = 0;
    ebufop = 0;
    return 0;
}

static int waitb(unsigned char ec) {
    int n;
    (void)ec;
    if (sock < 0 || !inited) return -ENODEV;
    ebufip = ebufil = 0;
    n = recvfrom(sock, (char *)ebufi + 2, sizeof(ebufi) - 2, MSG_NOSIGNAL, (struct sockaddr *)&eclient, &fromlen);
    if (n < 0) return -EIO;
    ebufi[0] = ntohs(eclient.sin_port) >> 8;
    ebufi[1] = ntohs(eclient.sin_port);
    eclient.sin_addr.s_addr = sin_addr.s_addr;
    driver_errno = 0;
    ebufip = ebufi[0] ? 0 : 2;
    ebufil = n + 2;
    return ebufi[ebufip++];
}

static const Driver driver = {
    .name         = "ETH",
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

const Driver *eth_driver(const char *addr, int network) {
    i_addr = addr != NULL ? addr : CLIENT_IP;
    i_network = network;
    log_printf("Using %s driver connecting to %s network %d", driver.name, i_addr, i_network);
    return &driver;
}
