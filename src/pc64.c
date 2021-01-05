/*

 pc64.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "pc64.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "crc8.h"
#include "log.h"
#include "driver.h"
#include "parport.h"
#include "timeout.h"

#define DEFAULT_IOPORT 0x378

static const Driver *driverp;
static unsigned short int IOPORT, STATPORT;
static int pc64s;
static const char *i_dev;
static int i_port;
static int inited;
static int driver_errno;

static int busy_wait;

static int initialize(int lastfail) {
    int ret;
    inited = 0;

    ret = parport_init(i_dev, i_port, lastfail);
    if (ret != 0) return ret;
    timeout_init();
    inited = 1;
    return 0;
}

#ifdef WIN32
static int getbio32(int use_timeout) {
    unsigned char b, a;
    if (driver_errno != 0) return EOF;
    if (use_timeout) timeout_set(1);

    while (inp32(STATPORT) < 0x80 && !timeout_expired);
    if (!pc64s) {
        oup32(0x80, IOPORT);
        b = inp32(STATPORT);
        oup32(0x00, IOPORT);
        b = (b & 0x78) >> 3;
        while (inp32(STATPORT) >= 0x80 && !timeout_expired);
        oup32(0x80, IOPORT);
    } else {
        b = inp32(STATPORT);
        oup32(0x08, IOPORT);
        b = (b & 0x78) >> 3;
        while (inp32(STATPORT) >= 0x80 && !timeout_expired);
    }
    a = inp32(STATPORT);
    oup32(0x00, IOPORT);
    b |= (a & 0x78) << 1;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendbio32(unsigned char b) {
    unsigned char a;
    if (driver_errno != 0) return;
    crc_add_byte(b);
    timeout_set(1);
    if (!pc64s) {
        a = (b & 0x0f) | 0x80;
        while (inp32(STATPORT) < 0x80 && !timeout_expired);
        oup32(a, IOPORT);
        oup32(a & 0x0f, IOPORT);
        b = (b >> 4) | 0x80;
        while (inp32(STATPORT) >= 0x80 && !timeout_expired);
        oup32(b, IOPORT);
        oup32(b & 0x0f, IOPORT);
    } else {
        a = (b & 0x07);
        while (inp32(STATPORT) < 0x80 && !timeout_expired);
        oup32(a, IOPORT);
        oup32(a | 8, IOPORT);
        a = (((b >> 3) & 3) | (b & 0x04)) ^ 0x0d;
        while (inp32(STATPORT) >= 0x80 && !timeout_expired);
        oup32(a, IOPORT);
        a = (b >> 5) ^ 3 ^ ((b >> 2) & 1);
        while (inp32(STATPORT) & 0x08 && !timeout_expired);
        oup32(a, IOPORT);
    }
    if (timeout_expired) driver_errno = -EIO;
}

static int waitbio32(unsigned char ec) {
    int dyntime = 1;
    int i;
    (void)ec;
    if (!inited) return -ENODEV;
    do {
        timeout_cancel();
        oup32(inp32(IOPORT) & (~8), IOPORT);
        do {
            while (inp32(STATPORT) < 0x80) {
                if (busy_wait) continue;
                usleep(dyntime);
                if (dyntime < 16384) dyntime <<= 1;
            }
        } while ((inp32(STATPORT) & 0x78) == 0);
        timeout_expired = 0;
        driver_errno = 0;
        i = getbio32(1);
    } while (i == EOF);
    timeout_cancel();
    return (unsigned char)i;
}
#elif defined __FreeBSD__
#elif defined PARPORT
static int getbparport(int use_timeout) {
    unsigned char b, a;
    unsigned char c;
    if (driver_errno != 0) return EOF;
    if (use_timeout) timeout_set(1);

    do {
        ioctl(parportfd, PPRSTATUS, &b);
        if (b & PARPORT_STATUS_BUSY) break;
    } while (!timeout_expired);
    if (!pc64s) {
        c = 0x80; ioctl(parportfd, PPWDATA, &c);
        ioctl(parportfd, PPRSTATUS, &b);
        c = 0x00; ioctl(parportfd, PPWDATA, &c);
    } else {
        ioctl(parportfd, PPRSTATUS, &b);
        c = 0x08; ioctl(parportfd, PPWDATA, &c);
    }
    b = (b & 0x78) >> 3;
    do {
        ioctl(parportfd, PPRSTATUS, &a);
        if (!(a & PARPORT_STATUS_BUSY)) break;
    } while (!timeout_expired);
    if (!pc64s) {
        c = 0x80; ioctl(parportfd, PPWDATA, &c);
    }
    ioctl(parportfd, PPRSTATUS, &a);
    c = 0x00; ioctl(parportfd, PPWDATA, &c);
    b |= (a & 0x78) << 1;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendbparport(unsigned char b) {
    unsigned char a;
    if (driver_errno != 0) return;
    crc_add_byte(b);
    timeout_set(1);
    if (!pc64s) {
        a = (b & 0x0f) | 0x80;
    } else {
        a = (b & 0x07);
    }
    do {
        unsigned char c;
        ioctl(parportfd, PPRSTATUS, &c);
        if (c & PARPORT_STATUS_BUSY) break;
    } while (!timeout_expired);
    ioctl(parportfd, PPWDATA, &a);
    if (!pc64s) {
        a &= 0x0f; ioctl(parportfd, PPWDATA, &a);
        b = (b >> 4) | 0x80;
    } else {
        a |= 8; ioctl(parportfd, PPWDATA, &a);
        a = (((b >> 3) & 3) | (b & 0x04)) ^ 0x0d;
    }
    do {
        unsigned char c;
        ioctl(parportfd, PPRSTATUS, &c);
        if (!(c & PARPORT_STATUS_BUSY)) break;
    } while (!timeout_expired);
    if (!pc64s) {
        ioctl(parportfd, PPWDATA, &b);
        b &= 0x0f; ioctl(parportfd, PPWDATA, &b);
    } else {
        ioctl(parportfd, PPWDATA, &a);
        a = (b >> 5) ^ 3 ^ ((b >> 2) & 1);
        do {
            unsigned char c;
            ioctl(parportfd, PPRSTATUS, &c);
            if (!(c & PARPORT_STATUS_ERROR)) break;
        } while (!timeout_expired);
        ioctl(parportfd, PPWDATA, &a);
    }
    if (timeout_expired) driver_errno = -EIO;
}
#endif

static int getb(int use_timeout) {
    unsigned char b, a;
    if (driver_errno != 0) return EOF;
    if (use_timeout) timeout_set(1);

    while (inb(STATPORT) < 0x80 && !timeout_expired);
    if (!pc64s) {
        outb(0x80, IOPORT);
        b = inb(STATPORT);
        outb(0x00, IOPORT);
        b = (b & 0x78) >> 3;
        while (inb(STATPORT) >= 0x80 && !timeout_expired);
        outb(0x80, IOPORT);
    } else {
        b = inb(STATPORT);
        outb(0x08, IOPORT);
        b = (b & 0x78) >> 3;
        while (inb(STATPORT) >= 0x80 && !timeout_expired);
    }
    a = inb(STATPORT);
    outb(0x00, IOPORT);
    b |= (a & 0x78) << 1;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendb(unsigned char b) {
    unsigned char a;
    if (driver_errno != 0) return;
    crc_add_byte(b);
    timeout_set(1);
    if (!pc64s) {
        a = (b & 0x0f) | 0x80;
        while (inb(STATPORT) < 0x80 && !timeout_expired);
        outb(a, IOPORT);
        outb(a & 0x0f, IOPORT);
        b = (b >> 4) | 0x80;
        while (inb(STATPORT) >= 0x80 && !timeout_expired);
        outb(b, IOPORT);
        outb(b & 0x0f, IOPORT);
    } else {
        a = (b & 0x07);
        while (inb(STATPORT) < 0x80 && !timeout_expired);
        outb(a, IOPORT);
        outb(a | 8, IOPORT);
        a = (((b >> 3) & 3) | (b & 0x04)) ^ 0x0d;
        while (inb(STATPORT) >= 0x80 && !timeout_expired);
        outb(a, IOPORT);
        a = (b >> 5) ^ 3 ^ ((b >> 2) & 1);
        while (inb(STATPORT) & 0x08 && !timeout_expired);
        outb(a, IOPORT);
    }
    if (timeout_expired) driver_errno = -EIO;
}

static void getbytes(unsigned char data[], unsigned int bytes) {
    unsigned int i;
    if (driver_errno != 0) return;
    for (i = 0; i < bytes; i++) {
        int j = driverp->getb((i & 255) == 0);
        if (j == EOF) return;
        data[i] = j;
    }
}

static void sendbytes(const unsigned char data[], unsigned int bytes) {
    unsigned int i;
    for (i = 0; i < bytes; i++) {
        driverp->sendb(data[i]);
    }
}

static void eshutdown(void) {
    parport_deinit();
    timeout_deinit();
    inited = 0;
    return;
}

static int flush(void) {
    timeout_cancel();
    return driver_errno;
}

static int done(void) {
    timeout_cancel();
    return driver_errno;
}

static void turn(void) {
}

static int clean(void) {
    return timeout_expired;
}

static int waitb(unsigned char ec) {
    int dyntime = 1;
    int i;
    (void)ec;
    if (!inited) return -ENODEV;
    do {
        timeout_cancel();
#ifdef PARPORT
        if (parportfd >= 0) {
            unsigned char b;
            ioctl(parportfd, PPRDATA, &b);
            b &= ~8; ioctl(parportfd, PPWDATA, &b);
            for (;;) {
                ioctl(parportfd, PPRSTATUS, &b);
                if (b & PARPORT_STATUS_BUSY) {
                    ioctl(parportfd, PPRSTATUS, &b);
                    if (b & 0x78) break;
                }
                if (busy_wait) continue;
                usleep(dyntime);
                if (dyntime < 16384) dyntime <<= 1;
            }
        } else
#endif
        {
            outb(inb(IOPORT) & (~8), IOPORT);
            do {
                while (inb(STATPORT) < 0x80) {
                    if (busy_wait) continue;
                    usleep(dyntime);
                    if (dyntime < 16384) dyntime <<= 1;
                }
            } while ((inb(STATPORT) & 0x78) == 0);
        }
        timeout_expired = 0;
        driver_errno = 0;
        i = driverp->getb(1);
    } while (i == EOF);
    timeout_cancel();
    return (unsigned char)i;
}

static Driver driver = {
    .name         = "PC64",
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

static const Driver *pc64_driver_generic(void) {
    driverp = &driver;
#ifdef WIN32
    if (parport_libinit()) {
        driver.getb = getbio32;
        driver.sendb = sendbio32;
        driver.wait = waitbio32;
    } else {
        driver.getb = getb;
        driver.sendb = sendb;
        driver.wait = waitb;
    }
#elif defined __FreeBSD__
#elif defined __DJGPP__
#else
#ifdef PARPORT
    if (i_dev != NULL) {
        driver.getb = getbparport;
        driver.sendb = sendbparport;
    } else {
        driver.getb = getb;
        driver.sendb = sendb;
    }
#endif
#endif

    if (i_dev != NULL) {
        log_printf("Using %s driver on device %s", driver.name, i_dev);
    } else {
        log_printf("Using %s driver at port %x", driver.name, i_port);
    }
    return &driver;
}

const Driver *pc64_driver(const char *dev, int port, int hog) {
    busy_wait = hog;
    i_dev = dev;
    pc64s = 0;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    IOPORT = port;
    STATPORT = port + 1;
    driver.name = "PC64";
    return pc64_driver_generic();
}

const Driver *pc64s_driver(const char *dev, int port, int hog) {
    busy_wait = hog;
    i_dev = dev;
    pc64s = 1;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    IOPORT = port;
    STATPORT = port + 1;
    driver.name = "PC64S";
    return pc64_driver_generic();
}

