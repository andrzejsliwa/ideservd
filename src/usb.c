/*

 usb.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "usb.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "crc8.h"
#include "log.h"
#include "driver.h"
#include "timeout.h"

#ifndef WIN32
#include <ftdi.h>

static struct ftdi_context *ftDevice;
#else
#include <windows.h>    //also in ideservd.h, but we need it now
#include "ftd2xx.h"

FT_HANDLE ftHandle;
#endif //WIN32

static const char *i_dev;
static int inited;
static int driver_errno;
static unsigned char ebufo[4096];
static unsigned int ebufop;
static int last_ec;

static int flush(void);

static int initialize(int lastfail) {
#ifndef WIN32
    struct ftdi_device_list *list, *i, *j;
    char desc[256], serial[256];

    inited = 0;

    ftDevice = ftdi_new();
    if (!ftDevice) {
        if (lastfail != -1) log_print("Out of memory");
        return -1;
    }

    if (ftdi_usb_find_all(ftDevice, &list, 0x0403, 0x6001) < 0) {
        if (lastfail != -2) log_printf("ftdi_usb_find_all: %s", ftdi_get_error_string(ftDevice));
        ftdi_free(ftDevice);
        ftDevice = NULL;
        return -2;
    }

    for (i = list; i; i = i->next) {
        if (ftdi_usb_get_strings(ftDevice, i->dev, NULL, 0, desc, sizeof desc, serial, sizeof serial) < 0) {
            continue;
        }
        if (i_dev != NULL) {
            if (!strcmp(i_dev, serial)) break;
        } else {
            if (!strcmp(desc, "IDE64 USB DEVICE")) break;
        }
    }
    if (!i && i_dev == NULL) {
        for (i = list; i; i = i->next) {
            if (ftdi_usb_get_strings(ftDevice, i->dev, NULL, 0, desc, sizeof desc, serial, sizeof serial) < 0) {
                continue;
            }
            if (!strcmp(desc, "FT245R USB FIFO")) break;
        }
    }
    for (j = list; j; j = j->next) {
        char desc2[256], serial2[256];
        if (i == j) {
            continue;
        }
        if (ftdi_usb_get_strings(ftDevice, j->dev, NULL, 0, desc2, sizeof desc2, serial2, sizeof serial2) < 0) {
            continue;
        }
        log_printf("Ignored device: %s (%s)", serial2, desc2);
    }
    if (!i) {
        ftdi_list_free(&list);
        if (lastfail != -3) log_print("No usable USB device found for now");
        ftdi_free(ftDevice);
        ftDevice = NULL;
        return -3;
    }
    if (ftdi_usb_open_dev(ftDevice, i->dev)) {
        ftdi_list_free(&list);
        if (lastfail != -4) log_printf("ftdi_usb_open_dev: %s", ftdi_get_error_string(ftDevice));
        ftdi_free(ftDevice);
        ftDevice = NULL;
        return -4;
    }
    if (ftdi_usb_reset(ftDevice)) {
        ftdi_list_free(&list);
        if (lastfail != -5) log_printf("ftdi_usb_reset: %s", ftdi_get_error_string(ftDevice));
        ftdi_free(ftDevice);
        ftDevice = NULL;
        return -5;
    }
    log_printf("Served device: %s (%s)", serial, desc);
    ftdi_list_free(&list);

    ftdi_usb_purge_buffers(ftDevice);
    ftdi_set_latency_timer(ftDevice, 4);
#else
    DWORD n, i, j;
    FT_DEVICE_LIST_INFO_NODE *list;
    FT_STATUS ftStatus;

    inited = 0;

    ftStatus = FT_CreateDeviceInfoList(&n);
    if (ftStatus != FT_OK) {
        if (lastfail != -6) log_print("FT_CreateDeviceInfoList failed");
        return -6;
    }
    if (n == 0) {
        if (lastfail != -3) log_print("No usable USB device found for now");
        return -3;
    }

    list = (FT_DEVICE_LIST_INFO_NODE *)malloc(n * sizeof *list);
    if (!list) {
        if (lastfail != -1) log_print("Out of memory");
        return -1;
    }
    ftStatus = FT_GetDeviceInfoList(list, &n);
    if (ftStatus != FT_OK) n = 0;
    for (i = 0; i < n; i++) {
        if (i_dev != NULL) {
            if (!strcmp(i_dev, list[i].SerialNumber)) break;
        } else {
            if (!strcmp("IDE64 USB DEVICE", list[i].Description)) break;
        }
    }
    if (i == n && i_dev == NULL) {
        for (i = 0; i < n; i++) {
            if (!strcmp("FT245R USB FIFO", list[i].Description)) break;
        }
    }
    if (i == n) {
        free(list);
        if (lastfail != -3) log_print("No usable USB device found for now");
        return -3;
    }
    ftStatus = FT_OpenEx((void*)list[i].LocId, FT_OPEN_BY_LOCATION, &ftHandle);
    if (ftStatus != FT_OK) ftStatus = FT_OpenEx(list[i].SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
    if (ftStatus != FT_OK) {
        free(list);
        if (lastfail != -7) log_print("FT_OpenEx failed");
        return -7;
    }
    log_printf("Served device: %s (%s)", list[i].SerialNumber, list[i].Description);
    for (j = 0; j < n; j++) {
        if (i == j) {
            continue;
        }
        log_printf("Ignored device: %s (%s)", list[j].SerialNumber, list[j].Description);
    }
    free(list);

    FT_ResetDevice(ftHandle);
    FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    FT_SetTimeouts(ftHandle, 100, 10000);       // C64->PC, PC->C64
    FT_SetLatencyTimer(ftHandle, 1);
//     FT_SetUSBParameters(ftHandle, 64,0);
//     FT_SetDeadmanTimeout(ftHandle, 10);
#endif
    timeout_init();
    last_ec = -1;
    inited = 1;
    return 0;
}

static int getb(int use_timeout) {
    unsigned char data;
    if (driver_errno != 0) return EOF;
    if (use_timeout) timeout_set(1);
    do {
#ifndef WIN32
        int err = ftdi_read_data(ftDevice, &data, 1);
        if (err == -ENODEV) {
            driver_errno = -ENODEV;
            return EOF;
        }
        if (err < 0) {
            driver_errno = -EIO;
            return EOF;
        }
        if (err == 1) {
            crc_add_byte(data);
            return data;
        }
#else
        DWORD dwBytesRead;
        FT_STATUS ftStatus = FT_Read(ftHandle, &data, 1, &dwBytesRead);
        if (ftStatus == FT_DEVICE_NOT_FOUND) {
            driver_errno = -ENODEV;
            return EOF;
        }
        if (ftStatus != FT_OK) {
            driver_errno = -ENODEV;
            return EOF;
        }
        if (dwBytesRead == 1) {
            crc_add_byte(data);
            return data;
        }
#endif
    } while (!timeout_expired);
    driver_errno = -EIO;
    return EOF;
}

static void getbytes(unsigned char data[], unsigned int bytes) {
    int err = 1;
#ifdef WIN32
    DWORD dwBytesRead;
    FT_STATUS ftStatus;
#endif
    if (driver_errno != 0) return;
    do {
        unsigned int l;
        if (err != 0) timeout_set(1);
        if (bytes > 4096) l = 4096; else l = bytes;
        if (timeout_expired) {
            driver_errno = -EIO;
            return;
        }
#ifndef WIN32
        err = ftdi_read_data(ftDevice, data, l);
        if (err == -ENODEV) {
            driver_errno = -ENODEV;
            return;
        }
        if (err < 0) {
            driver_errno = -EIO;
            return;
        }
        crc_add_block(data, err);
        bytes -= err; data += err;
#else
        ftStatus = FT_Read(ftHandle, data, l, &dwBytesRead);
        if (ftStatus == FT_DEVICE_NOT_FOUND) {
            driver_errno = -ENODEV;
            return;
        }
        if (ftStatus != FT_OK) {
            driver_errno = -EIO;
            return;
        }
        crc_add_block(data, dwBytesRead);
        bytes -= dwBytesRead; data += dwBytesRead;
        err = (int)dwBytesRead;
#endif
    } while (bytes > 0);
}

static void sendb(unsigned char data) {
    if (ebufop >= sizeof ebufo) {
        driver_errno = flush();
        if (driver_errno != 0) return;
    }
    crc_add_byte(data);
    ebufo[ebufop++] = data;
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
#ifndef WIN32
    if (ftDevice) {
        ftdi_usb_purge_buffers(ftDevice);
        ftdi_usb_close(ftDevice);
        ftdi_free(ftDevice);
        ftDevice = NULL;
    }
#else
    FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    FT_Close(ftHandle);
#endif
    timeout_deinit();
    inited = 0;
}

static int flush(void) {
    if (ebufop && driver_errno == 0) {
        int l = ebufop;
        unsigned char *data = ebufo;
        ebufop = 0;
        do {
#ifndef WIN32
            int err = ftdi_write_data(ftDevice, data, l);
            if (err == -ENODEV) return -ENODEV;
            if (err < 0) return -EIO;
            l -= err; data += err;
#else
            DWORD dwBytesWritten;
            FT_STATUS ftStatus = FT_Write(ftHandle, data, l, &dwBytesWritten);
            if (ftStatus == FT_DEVICE_NOT_FOUND) return -ENODEV;
            if (ftStatus != FT_OK) return -EIO;
            l -= dwBytesWritten; data += dwBytesWritten;
#endif
        } while (l > 0);
    }
    return driver_errno;
}

static int done(void) {
    timeout_cancel();
    return driver_errno;
}

static void turn(void) {
    if (last_ec != -1) {
#ifndef WIN32
        ftdi_set_event_char(ftDevice, 0, 0);
#else
        FT_SetChars(ftHandle, 0, 0, 0, 0);
#endif
        last_ec = -1;
    }
}

static int clean(void) {
#ifndef WIN32
    ftdi_usb_purge_buffers(ftDevice);
#else
    FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
#endif
    ebufop = 0;
    return timeout_expired;
}

static int waitb(unsigned char ec) {
    int dyntime = 1;
    unsigned char data;
    if (!inited) return -ENODEV;
    timeout_cancel();
    if (ec != last_ec) {
#ifndef WIN32
        ftdi_set_event_char(ftDevice, ec, 1);
#else
        FT_SetChars(ftHandle, ec, 1, 0, 0);
#endif
        last_ec = ec;
    }
    for (;;) {
#ifndef WIN32
        int err = ftdi_read_data(ftDevice, &data, 1);
        if (err == -ENODEV) return -ENODEV;
        if (err < 0) return -EIO;
        if (err == 1) {
            break;
        }
#else
        DWORD dwBytesRead;
        FT_STATUS ftStatus = FT_Read(ftHandle, &data, 1, &dwBytesRead);
        if (ftStatus == FT_DEVICE_NOT_FOUND) return -ENODEV;
        if (ftStatus != FT_OK) return -ENODEV;
        if (dwBytesRead == 1) {
            break;
        }
#endif
        usleep(dyntime);
        if (dyntime < 16384) dyntime <<= 1;
    }
    driver_errno = 0; timeout_expired = 0;
    crc_add_byte(data);
    return data;
}

static const Driver driver = {
    .name         = "USB",
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

const Driver *usb_driver(const char *dev) {
    i_dev = dev;
    log_printf("Using %s driver for device %s", driver.name, dev != NULL ? dev : "*");
    return &driver;
}

