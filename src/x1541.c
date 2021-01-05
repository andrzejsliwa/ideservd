/*

 x1541.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "x1541.h"
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
static unsigned short int CTRLPORT, PORTIN;
static int inv;
static const char *i_dev;
static int i_port;
static int inited;
static int driver_errno;
static unsigned char DATALO, DATAHI, CLKLO, CLKHI, ATNHI, DATA_, CLK_, ATN_, DATA;
#define CLK 2

#ifdef PARPORT
static int parportin;
#endif

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
static inline void watnio32(int i) {
    unsigned char b;
    do {
        b = inp32(PORTIN);
        if (((b ^ inv) & ATN_) != i) return;
    } while (!timeout_expired);
}

static int getbio32(int use_timeout) {
    const unsigned char dl = DATALO + CLKHI + ATNHI, dh = DATAHI + CLKHI + ATNHI;
    unsigned char b = 0, c;
    if (driver_errno != 0) return EOF;
    oup32(dl, CTRLPORT);
    if (use_timeout) timeout_set(1);

    watnio32(0);
    if (inp32(PORTIN) & CLK_) b |= 128;
    oup32(dh, CTRLPORT);

    watnio32(ATN_);
    c = inp32(PORTIN);
    if (c & CLK_) b |= 64;
    oup32(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnio32(0);
    if (inp32(PORTIN) & CLK_) b |= 32;
    oup32(dh, CTRLPORT);

    watnio32(ATN_);
    c = inp32(PORTIN);
    if (c & CLK_) b |= 16;
    oup32(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnio32(0);
    if (inp32(PORTIN) & CLK_) b |= 8;
    oup32(dh, CTRLPORT);

    watnio32(ATN_);
    c = inp32(PORTIN);
    if (c & CLK_) b |= 4;
    oup32(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnio32(0);
    if (inp32(PORTIN) & CLK_) b |= 2;
    oup32(dh, CTRLPORT);

    watnio32(ATN_);
    c = inp32(PORTIN);
    if (c & CLK_) b |= 1;
    if ((c ^ inv) & DATA_) timeout_expired = 1;
    b ^= inv;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendbio32(unsigned char a) {
    int b, c;
    if (driver_errno != 0) return;
    b = a ^ (a << 1);
    crc_add_byte(a);
    c = a & 128 ? (CLKHI | ATNHI | DATALO) : (CLKLO | ATNHI | DATALO);
    oup32(c ^ DATA, CTRLPORT); oup32(c, CTRLPORT);
    timeout_set(1);
    if (b & 128) c ^= CLK;
    watnio32(0);
    if (b & 128) oup32(c, CTRLPORT);
    oup32(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 64) c ^= CLK;
    watnio32(ATN_);
    if (b & 64) oup32(c ^ DATA, CTRLPORT);
    oup32(c, CTRLPORT);
    if (b & 32) c ^= CLK;
    watnio32(0);
    if (b & 32) oup32(c, CTRLPORT);
    oup32(c ^ DATA, CTRLPORT);
    if (b & 16) c ^= CLK;
//    if (in() & DATA_) timeout_expired=1;
    watnio32(ATN_);
    if (b & 16) oup32(c ^ DATA, CTRLPORT);
    oup32(c, CTRLPORT);
    if (b & 8) c ^= CLK;
    watnio32(0);
    if (b & 8) oup32(c, CTRLPORT);
    oup32(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 4) c ^= CLK;
    watnio32(ATN_);
    if (b & 4) oup32(c ^ DATA, CTRLPORT);
    oup32(c, CTRLPORT);
    if (b & 2) c ^= CLK;
    watnio32(0);
    if (b & 2) oup32(c, CTRLPORT);
    oup32(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    watnio32(ATN_);
    if (timeout_expired) driver_errno = -EIO;
}

static int waitbio32(unsigned char ec) {
    int dyntime = 1;
    int i;
    (void)ec;
start:
    timeout_cancel();
    if (!inited) return -ENODEV;
    oup32(CLKLO + DATAHI + ATNHI, CTRLPORT);
    while (((inp32(PORTIN) ^ inv) & DATA_) == 0) {
        if (busy_wait) continue;
        usleep(dyntime);
        if (dyntime < 16384) dyntime <<= 1;
    }
    oup32(CLKHI + DATAHI + ATNHI, CTRLPORT);
    timeout_expired = 0; timeout_set(1);
    while (((inp32(PORTIN) ^ inv) & DATA_) != 0) {
        if (!busy_wait) {
            usleep(dyntime);
            if (dyntime < 16384) dyntime <<= 1;
        }
        if (timeout_expired) goto start;
    } //set clklo, wait atnlo
    driver_errno = 0;
    i = getbio32(1);
    if (i == EOF) goto start;
    timeout_cancel();
    return (unsigned char)i;
}
#elif defined __FreeBSD__
#elif defined PARPORT
static inline void watnparport(int i) {
    unsigned char b;
    do {
        ioctl(parportfd, parportin, &b);
        if (((b ^ inv) & ATN_) != i) return;
    } while (!timeout_expired);
}

static int getbparport(int use_timeout) {
    unsigned char dl = DATALO + CLKHI + ATNHI, dh = DATAHI + CLKHI + ATNHI;
    unsigned char b = 0, c;
    if (driver_errno != 0) return EOF;
    ioctl(parportfd, PPWCONTROL, &dl);
    if (use_timeout) timeout_set(1);

    watnparport(0);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 128;
    ioctl(parportfd, PPWCONTROL, &dh);

    watnparport(ATN_);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 64;
    ioctl(parportfd, PPWCONTROL, &dl);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnparport(0);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 32;
    ioctl(parportfd, PPWCONTROL, &dh);

    watnparport(ATN_);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 16;
    ioctl(parportfd, PPWCONTROL, &dl);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnparport(0);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 8;
    ioctl(parportfd, PPWCONTROL, &dh);

    watnparport(ATN_);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 4;
    ioctl(parportfd, PPWCONTROL, &dl);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watnparport(0);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 2;
    ioctl(parportfd, PPWCONTROL, &dh);

    watnparport(ATN_);
    ioctl(parportfd, parportin, &c);
    if (c & CLK_) b |= 1;
    if ((c ^ inv) & DATA_) timeout_expired = 1;
    b ^= inv;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendbparport(unsigned char a) {
    int b, c;
    if (driver_errno != 0) return;
    b = a ^ (a << 1);
    crc_add_byte(a);
    c = a & 128 ? (CLKHI | ATNHI | DATAHI) : (CLKLO | ATNHI | DATAHI);
    ioctl(parportfd, PPWCONTROL, &c); c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
    timeout_set(1);
    if (b & 128) c ^= CLK;
    watnparport(0);
    if (b & 128) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 64) c ^= CLK;
    watnparport(ATN_);
    if (b & 64) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
    if (b & 32) c ^= CLK;
    watnparport(0);
    if (b & 32) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
    if (b & 16) c ^= CLK;
//    if (in() & DATA_) timeout_expired=1;
    watnparport(ATN_);
    if (b & 16) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
    if (b & 8) c ^= CLK;
    watnparport(0);
    if (b & 8) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 4) c ^= CLK;
    watnparport(ATN_);
    if (b & 4) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
    if (b & 2) c ^= CLK;
    watnparport(0);
    if (b & 2) ioctl(parportfd, PPWCONTROL, &c);
    c ^= DATA; ioctl(parportfd, PPWCONTROL, &c);
//    if (in() & DATA_) timeout_expired=1;
    watnparport(ATN_);
    if (timeout_expired) driver_errno = -EIO;
}
#endif

static inline void watn(int i) {
    unsigned char b;
    do {
        b = inb(PORTIN);
        if (((b ^ inv) & ATN_) != i) return;
    } while (!timeout_expired);
}

static int getb(int use_timeout) {
    const unsigned char dl = DATALO + CLKHI + ATNHI, dh = DATAHI + CLKHI + ATNHI;
    unsigned char b = 0, c;
    if (driver_errno != 0) return EOF;
    outb(dl, CTRLPORT);
    if (use_timeout) timeout_set(1);

    watn(0);
    if (inb(PORTIN) & CLK_) b |= 128;
    outb(dh, CTRLPORT);

    watn(ATN_);
    c = inb(PORTIN);
    if (c & CLK_) b |= 64;
    outb(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watn(0);
    if (inb(PORTIN) & CLK_) b |= 32;
    outb(dh, CTRLPORT);

    watn(ATN_);
    c = inb(PORTIN);
    if (c & CLK_) b |= 16;
    outb(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watn(0);
    if (inb(PORTIN) & CLK_) b |= 8;
    outb(dh, CTRLPORT);

    watn(ATN_);
    c = inb(PORTIN);
    if (c & CLK_) b |= 4;
    outb(dl, CTRLPORT);
    if ((c ^ inv) & DATA_) timeout_expired = 1;

    watn(0);
    if (inb(PORTIN) & CLK_) b |= 2;
    outb(dh, CTRLPORT);

    watn(ATN_);
    c = inb(PORTIN);
    if (c & CLK_) b |= 1;
    if ((c ^ inv) & DATA_) timeout_expired = 1;
    b ^= inv;
    crc_add_byte(b);
    if (timeout_expired) {
        driver_errno = -EIO;
        return EOF;
    }
    return b;
}

static void sendb(unsigned char a) {
    int b, c;
    if (driver_errno != 0) return;
    b = a ^ (a << 1);
    crc_add_byte(a);
    c = a & 128 ? (CLKHI | ATNHI | DATALO) : (CLKLO | ATNHI | DATALO);
    outb(c ^ DATA, CTRLPORT); outb(c, CTRLPORT);
    timeout_set(1);
    if (b & 128) c ^= CLK;
    watn(0);
    if (b & 128) outb(c, CTRLPORT);
    outb(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 64) c ^= CLK;
    watn(ATN_);
    if (b & 64) outb(c ^ DATA, CTRLPORT);
    outb(c, CTRLPORT);
    if (b & 32) c ^= CLK;
    watn(0);
    if (b & 32) outb(c, CTRLPORT);
    outb(c ^ DATA, CTRLPORT);
    if (b & 16) c ^= CLK;
//    if (in() & DATA_) timeout_expired=1;
    watn(ATN_);
    if (b & 16) outb(c ^ DATA, CTRLPORT);
    outb(c, CTRLPORT);
    if (b & 8) c ^= CLK;
    watn(0);
    if (b & 8) outb(c, CTRLPORT);
    outb(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    if (b & 4) c ^= CLK;
    watn(ATN_);
    if (b & 4) outb(c ^ DATA, CTRLPORT);
    outb(c, CTRLPORT);
    if (b & 2) c ^= CLK;
    watn(0);
    if (b & 2) outb(c, CTRLPORT);
    outb(c ^ DATA, CTRLPORT);
//    if (in() & DATA_) timeout_expired=1;
    watn(ATN_);
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
start:
    timeout_cancel();
    if (!inited) return -ENODEV;
#ifdef PARPORT
    if (parportfd >= 0) {
        unsigned char c;
        c = CLKLO + DATAHI + ATNHI; ioctl(parportfd, PPWCONTROL, &c);
        for (;;) {
            ioctl(parportfd, parportin, &c);
            if ((c ^ inv) & DATA_) break;
            if (busy_wait) continue;
            usleep(dyntime);
            if (dyntime < 16384) dyntime <<= 1;
        }
        c = CLKHI + DATAHI + ATNHI; ioctl(parportfd, PPWCONTROL, &c);
        timeout_expired = 0; timeout_set(1);
        for (;;) {
            ioctl(parportfd, parportin, &c);
            if (((c ^ inv) & DATA_) == 0) break;
            if (!busy_wait) {
                usleep(dyntime);
                if (dyntime < 16384) dyntime <<= 1;
            }
            if (timeout_expired) goto start;
        } //set clklo, wait atnlo
    } else
#endif
    {
        outb(CLKLO + DATAHI + ATNHI, CTRLPORT);
        while (((inb(PORTIN) ^ inv) & DATA_) == 0) {
            if (busy_wait) continue;
            usleep(dyntime);
            if (dyntime < 16384) dyntime <<= 1;
        }
        outb(CLKHI + DATAHI + ATNHI, CTRLPORT);
        timeout_expired = 0; timeout_set(1);
        while (((inb(PORTIN) ^ inv) & DATA_) != 0) {
            if (!busy_wait) {
                usleep(dyntime);
                if (dyntime < 16384) dyntime <<= 1;
            }
            if (timeout_expired) goto start;
        } //set clklo, wait atnlo
    }
    driver_errno = 0;
    i = driverp->getb(1);
    if (i == EOF) goto start;
    timeout_cancel();
    return (unsigned char)i;
}

static Driver driver = {
    .name         = "IEC",
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

static const Driver *x1541_driver_generic(void) {
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

const Driver *x1541_driver(const char *dev, int port, int hog) {
    busy_wait = hog;
    i_dev = dev;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    CTRLPORT = port + 2;
    DATAHI = 0; DATALO = 8; DATA = 8;
    CLKHI = 0; CLKLO = 2;
    ATNHI = 0;
    DATA_ = 8; CLK_ = 2; ATN_ = 1;
    inv = 0; PORTIN = CTRLPORT;
#ifdef PARPORT
    parportin = PPRCONTROL;
#endif
    driver.name = "X1541";
    return x1541_driver_generic();
}

const Driver *xe1541_driver(const char *dev, int port, int hog) {
    int statport;
    busy_wait = hog;
    i_dev = dev;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    statport = port + 1;
    CTRLPORT = port + 2;
    DATAHI = 0; DATALO = 8; DATA = 8;
    CLKHI = 0; CLKLO = 2;
    ATNHI = 0;
    DATA_ = 0x80; CLK_ = 0x20; ATN_ = 0x10;
    inv = -1; PORTIN = statport;
#ifdef PARPORT
    parportin = PPRSTATUS;
#endif
    driver.name = "XE1541";
    return x1541_driver_generic();
}

const Driver *xm1541_driver(const char *dev, int port, int hog) {
    int statport;
    busy_wait = hog;
    i_dev = dev;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    statport = port + 1;
    CTRLPORT = port + 2;
    DATAHI = 4; DATALO = 0; DATA = 4;
    CLKHI = 0; CLKLO = 2;
    ATNHI = 0;
    DATA_ = 0x40; CLK_ = 0x20; ATN_ = 0x10;
    inv = -1; PORTIN = statport;
#ifdef PARPORT
    parportin = PPRSTATUS;
#endif
    driver.name = "XM1541";
    return x1541_driver_generic();
}

const Driver *xa1541_driver(const char *dev, int port, int hog) {
    int statport;
    busy_wait = hog;
    i_dev = dev;
    if (port == 0) port = DEFAULT_IOPORT;
    i_port = port & 0xffff;
    statport = port + 1;
    CTRLPORT = port + 2;
    DATAHI = 0; DATALO = 4; DATA = 4;
    CLKHI = 2; CLKLO = 0;
    ATNHI = 1;
    DATA_ = 0x40; CLK_ = 0x20; ATN_ = 0x10;
    inv = -1; PORTIN = statport;
#ifdef PARPORT
    parportin = PPRSTATUS;
#endif
    driver.name = "XA1541";
    return x1541_driver_generic();
}
