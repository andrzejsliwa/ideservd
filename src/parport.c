/*

 parport.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "parport.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
#include <windows.h>
#define PORTTALK_TYPE 40000
#define IOCTL_IOPM_RESTRICT_ALL_ACCESS CTL_CODE(PORTTALK_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_IOPM CTL_CODE(PORTTALK_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#elif defined __FreeBSD__
#include <fcntl.h>
#elif defined __DJGPP__
#else
#endif

#include "log.h"

#ifdef WIN32
static HANDLE porttalk = INVALID_HANDLE_VALUE;
static HINSTANCE hLib;

inpfuncPtr inp32 = NULL;
oupfuncPtr oup32 = NULL;
#elif defined __FreeBSD__
static int iofd = -1;
#elif defined __DJGPP__
#else
#include <sys/ioctl.h>
#include <fcntl.h>

static int perm_port = 0;
#endif // WIN32

#ifdef PARPORT
int parportfd = -1;
#endif

#ifdef WIN32
int parport_libinit(void) {
    typedef BOOL (__stdcall *lpIsInpOutDriverOpen)(void);
    lpIsInpOutDriverOpen is_driver_open;
    /* Load the library */
    if (hLib == NULL) {
        hLib = LoadLibrary("inpout32.dll");
        if (hLib == NULL) {
            log_print("LoadLibrary of inpout32.dll failed");
            return 0;
        }
    }
    is_driver_open = (lpIsInpOutDriverOpen)GetProcAddress(hLib, "IsInpOutDriverOpen");
    if (is_driver_open != NULL && !is_driver_open()) {
        log_print("InpOut32 driver is not loaded");
        return 0;
    }
    if (inp32 == NULL) {
        /* get the address of the function */
        inp32 = (inpfuncPtr)GetProcAddress(hLib, "Inp32");
        if (inp32 == NULL) {
            log_print("GetProcAddress for Inp32 failed");
            return 0;
        }
    }
    if (oup32 == NULL) {
        oup32 = (oupfuncPtr)GetProcAddress(hLib, "Out32");
        if (oup32 == NULL) {
            log_print("GetProcAddress for Oup32 failed");
            return 0;
        }
    }
    return 1;
}
#endif

int parport_init(const char *device, int port, int lastfail) {
#ifdef WIN32
    (void)device;
    if (inp32 != NULL && oup32 != NULL) {
        return 0;
    }
    if (porttalk == INVALID_HANDLE_VALUE) {
        porttalk = CreateFile("\\\\.\\PortTalk", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (porttalk == INVALID_HANDLE_VALUE) {
            if (lastfail != -1) log_print("PortTalk driver not found");
            return -1;
        }
    }
    if (porttalk != INVALID_HANDLE_VALUE) {
        DWORD ret;
        DeviceIoControl(porttalk, IOCTL_IOPM_RESTRICT_ALL_ACCESS, NULL, 0, NULL, 0, &ret, NULL);
        if (DeviceIoControl(porttalk, IOCTL_SET_IOPM, &port, 3, NULL, 0, &ret, NULL) != 0) {
            if (lastfail != -2) log_print("PortTalk IOCTL_SET_IOPM failed");
            return -2;
        }
    }
#elif defined __FreeBSD__
    (void)device;
    (void)port;
    if (iofd == -1) {
        iofd = open("/dev/io", O_RDWR);
        if (iofd == -1) {
            if (lastfail != -1) log_printf("Error opening /dev/io: %s(%d)", strerror(errno), errno);
            return -1;
        }
    }
#elif defined __DJGPP__
    (void)device;
    (void)port;
    (void)lastfail;
#else
#ifdef PARPORT
    if (device != NULL && parportfd == -1) {
        int mode;
        parportfd = open(device, O_RDWR);
        if (parportfd == -1) {
            if (lastfail != -1) log_printf("Error opening %s parport device: %s(%d)", device, strerror(errno), errno);
            return -1;
        }
        if (ioctl(parportfd, PPEXCL) || ioctl(parportfd, PPCLAIM)) {
            close(parportfd);
            parportfd = -1;
            if (lastfail != -2) log_printf("Cannot access %s parport device: %s(%d)", device, strerror(errno), errno);
            return -2;
        }
        mode = IEEE1284_MODE_ECP;
        if (ioctl(parportfd, PPSETMODE, &mode)) {
            close(parportfd);
            parportfd = -1;
            if (lastfail != -3) log_printf("Cannot set byte mode on %s parport device: %s(%d)", device, strerror(errno), errno);
            return -3;
        }

        mode = 0;
        if (ioctl(parportfd, PPDATADIR, &mode)) {
            close(parportfd);
            parportfd = -1;
            if (lastfail != -4) log_printf("Cannot set direction on %s parport device: %s(%d)", device, strerror(errno), errno);
            return -4;
        }
    } else
#endif
    if (perm_port == 0) {
        if (ioperm(port, 3, 1)) {
            if (lastfail != -5) log_printf("Cannot allocate I/O ports %x to %x: %s(%d)", port, port + 2, strerror(errno), errno);
            return -5;
        } else perm_port = port;
    }
#endif
    return 0;
}

void parport_deinit(void) {
#ifdef WIN32
    if (porttalk != INVALID_HANDLE_VALUE) {
        DWORD ret;
        DeviceIoControl(porttalk, IOCTL_IOPM_RESTRICT_ALL_ACCESS, NULL, 0, NULL, 0, &ret, NULL);
        CloseHandle(porttalk);
        porttalk = INVALID_HANDLE_VALUE;
    }
#elif defined __FreeBSD__
    if (iofd >= 0) {
        close(iofd);
        iofd = -1;
    }
#elif defined __DJGPP__
#else
#ifdef PARPORT
    if (parportfd != -1) {
        ioctl(parportfd, PPRELEASE);
        close(parportfd);
        parportfd = -1;
    } else
#endif
    if (perm_port != 0) {
        ioperm(perm_port, 3, 0);
        perm_port = 0;
    }
#endif
}
