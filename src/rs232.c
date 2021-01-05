/*

 rs232.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "rs232.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>

static HANDLE commhandle;

#else
#include <sys/ioctl.h>
#include <termios.h>

static int commhandle;

#ifndef FNDELAY
#define FNDELAY 0
#endif

#endif

#include "crc8.h"
#include "log.h"
#include "driver.h"

#if defined WIN32 || defined __DJGPP__
#define DEFAULT_COMPORT "COM1"
#else
#define DEFAULT_COMPORT "/dev/ttyS0"
#endif

static const char *i_dev;
static int i_baudlow;
static int inited;
static int driver_errno;

static int initialize(int lastfail) {
#ifdef WIN32
    DCB dcb;
    COMMTIMEOUTS commtimeouts;

    inited = 0;
// Open the serial port
    commhandle = CreateFile(i_dev, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
// Initialize the DCBlength member
    dcb.DCBlength = sizeof (DCB);
// Get the default port setting information
    GetCommState (commhandle, &dcb);
//Change the DCB structure settings
    dcb.BaudRate = i_baudlow ? 38400 : 115200;// Current baud
    dcb.fBinary = TRUE;               	// Binary mode, (windows supports only binary)
    dcb.fParity = TRUE;               	// Enable parity checking 
    dcb.fOutxCtsFlow = TRUE;         	// CTS output flow control 
    dcb.fOutxDsrFlow = FALSE;         	// No DSR output flow control 
    dcb.fDtrControl = DTR_CONTROL_ENABLE; 	// Enable DTR signal while device open
    dcb.fDsrSensitivity = FALSE;      	// DSR sensitivity 
    dcb.fTXContinueOnXoff = TRUE;     	// XOFF continues Tx 
    dcb.fOutX = FALSE;                	// No XON/XOFF out flow control 
    dcb.fInX = FALSE;                 	// No XON/XOFF in flow control 
    dcb.fErrorChar = FALSE;           	// Disable error replacement 
    dcb.fNull = FALSE;                	// Disable null stripping 
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;// RTS flow control 
    dcb.fAbortOnError = FALSE;        	// Do not abort reads/writes on error
    dcb.ByteSize = 8;                 	// Number of bits/byte, 4-8 
    dcb.Parity = NOPARITY;            	// 0-4=no,odd,even,mark,space 
    dcb.StopBits = ONESTOPBIT;        	// 0,1,2 = 1, 1.5, 2 
// Configure the port according to the specifications of the DCB structure
    if (!SetCommState (commhandle, &dcb)) {
        if (lastfail != -1) log_printf("Unable to configure serial port %s", i_dev);
        return -1;
    }
// Retrieve the timeout parameters for all read and write operations on the port
    GetCommTimeouts (commhandle, &commtimeouts);

// Change the COMMTIMEOUTS structure settings
    commtimeouts.ReadIntervalTimeout = MAXDWORD;
    commtimeouts.ReadTotalTimeoutMultiplier = 0;
    commtimeouts.ReadTotalTimeoutConstant = 1000;       // MAXDWORD, 0, after x miliseconds read returns even if no bytes received
    commtimeouts.WriteTotalTimeoutMultiplier = 0;
    commtimeouts.WriteTotalTimeoutConstant = 1000;

// Set the timeout parameters for all read and write operations on the port
    if (!SetCommTimeouts (commhandle, &commtimeouts)) {
        if (lastfail != -2) log_printf("Unable to set the timeout parameters for serial port %s", i_dev);
        return -2;
    }
#else
    int flags;
    struct termios options;

    inited = 0;
    /// Changes by Silver Dream !
    if ((commhandle = open(i_dev, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
        if (lastfail != -3) log_printf("Cannot open %s: %s(%d)", i_dev, strerror(errno), errno);
        return -3;
    }
#ifdef TIOCEXCL
    if (ioctl(commhandle, TIOCEXCL) < 0) {
        if (lastfail != -4) log_printf("Error setting TIOCEXCL on %s: %s(%d)", i_dev, strerror(errno), errno);
        return -4;
    }
#endif
    if (fcntl(commhandle, F_SETFL, 0) < 0) {
        if (lastfail != -5) log_printf("Error clearing O_NONBLOCK %s: %s(%d)", i_dev, strerror(errno), errno);
        return -5;
    }

    if (tcgetattr(commhandle, &options) < 0) {
        if (lastfail != -6) log_printf("Cannot read params %s: %s(%d)", i_dev, strerror(errno), errno);
        return -6;
    }
    // End of changes by Silver Dream !

    /* baud rate */
#ifdef B115200
    if (!i_baudlow) {
        cfsetispeed(&options, B115200);
        cfsetospeed(&options, B115200);
    } else
#endif
    {
        cfsetispeed(&options, B38400);
        cfsetospeed(&options, B38400);
    }

    /* input options */

    options.c_iflag = 0;

    /* cout options */

    options.c_oflag = 0;

    /* control options */
    /* 8N1 */

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    /* CTS/RTS flow control */

//    if (flowctrl)
//    {
//	printf("Enabling RTS/CTS flow control\n");
#ifdef CRTSCTS
    options.c_cflag |= CRTSCTS;
#endif
//    } else options.c_cflag &= ~CRTSCTS;

    /* line options */
    /* raw input */

//  options.c_lflag |= IEXTEN; /* leave enabled to get flow control */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ISIG);

    /* control character */

//  options.c_cc[VMIN] = 1; /* minimum number of character to read */
    options.c_cc[VMIN] = 0; /* minimum number of character to read */
    options.c_cc[VTIME] = 10; /* time to wait for data */

    if (tcsetattr(commhandle, TCSANOW, &options) < 0) {
        if (lastfail != -7) log_printf("Cannot set params on %s: %s(%d)", i_dev, strerror(errno), errno);
        return -7;
    }

    /* turn turn O_NONBLOCK again for sure */
    /* O_NONBLOCK = O_NDELAY -> DOESNT CARE WHAT STATE DCD */


    flags = fcntl(commhandle, F_GETFL, 0);
    if (flags < 0) {
        if (lastfail != -8) log_printf("Error executing fcntl on %s: %s(%d)", i_dev, strerror(errno), errno);
        return -8;
    }
    if (fcntl(commhandle, F_SETFL, flags & ~(O_NONBLOCK | FNDELAY)) < 0) {
        if (lastfail != -9) log_printf("Error executing fcntl on %s: %s(%d)", i_dev, strerror(errno), errno);
        return -9;
    }

//fcntl(commhandle, F_SETFL, (flags | O_NONBLOCK) & ~FNDELAY);
/* set the read function to return immediately */

//  fcntl(commhandle, F_SETFL, FNDELAY);
#endif
    inited = 1;
    return 0;
}

static int getb(int use_timeout) {
    (void)use_timeout;
    unsigned char data;
    int err;

    if (driver_errno != 0) return EOF;
#ifndef WIN32
    err = read(commhandle, &data, 1);
    if (err < 0) {
        driver_errno = -EIO;
        return EOF;
    }
    if (err == 1) {
        crc_add_byte(data);
        return (unsigned char)data;
    }
#else
    DWORD byteswritten;

    err = ReadFile(commhandle, &data, 1, &byteswritten, NULL);
    if (err) {
        crc_add_byte(data);
        return (unsigned char)data;
    }
#endif
    driver_errno = -EBUSY;
    return EOF;
}

static void sendb(unsigned char a) {
#ifdef WIN32
    DWORD byteswritten;
#else
    int err;
#endif

    if (driver_errno != 0) return;
    crc_add_byte(a);
#ifndef WIN32
    do {
        err = write(commhandle, &a, 1);
        if (err < 0) {
            driver_errno = -EIO;
            return;
        }
    } while (err != 1);
#else
    if (WriteFile(commhandle, &a, 1, &byteswritten, NULL) == 0) {
        driver_errno = -EIO;
    }
#endif
}

static void getbytes(unsigned char data[], unsigned int bytes) {
    int err;
    if (driver_errno != 0) return;
    do {
#ifndef WIN32
        err = read(commhandle, data, bytes);
        if (err <= 0) {
            driver_errno = -EIO;
            return;
        }
        crc_add_block(data, err);
        bytes -= err; data += err;
#else
        DWORD dwBytesRead;
        err = ReadFile(commhandle, data, bytes, &dwBytesRead, NULL);
        if (!err || !dwBytesRead) {
            driver_errno = -EIO;
            return;
        }
        crc_add_block(data, dwBytesRead);
        bytes -= dwBytesRead; data += dwBytesRead;
#endif
    } while (bytes > 0);
}

static void sendbytes(const unsigned char data[], unsigned int bytes) {
    int err;
    if (driver_errno != 0) return;
    crc_add_block(data, bytes);
    do {
#ifndef WIN32
        err = write(commhandle, data, bytes);
        if (err < 0) {
            driver_errno = -EIO;
            return;
        }
        bytes -= err; data += err;
#else
        DWORD dwBytesWritten;
        err = WriteFile(commhandle, data, bytes, &dwBytesWritten, NULL);
        if (!err || !dwBytesWritten) {
            driver_errno = -EIO;
            return;
        }
        bytes -= dwBytesWritten; data += dwBytesWritten;
#endif
    } while (bytes > 0);
}

static void eshutdown(void) {
#ifndef WIN32
    tcflush(commhandle, TCIOFLUSH);
    close(commhandle);
#else
    CancelIo(commhandle);
    CloseHandle(commhandle);
#endif
    inited = 0;
    return;
}

static int flush(void) {
    int i;
    if (driver_errno == 0) {
#ifndef WIN32
        i = tcdrain(commhandle);
#else
        i = !FlushFileBuffers(commhandle);
#endif
        if (i != 0) return -EIO;
    }
    return driver_errno;
}

static int done(void) {
    return driver_errno;
}

static void turn(void) {
}

static int clean(void) {
#ifndef WIN32
    tcflush(commhandle, TCIOFLUSH);
#else
    CancelIo(commhandle);
#endif
    return 0;
}

static int waitb(unsigned char ec) {
    int b;
    (void)ec;
    if (!inited) return -ENODEV;
    do {
        driver_errno = 0;
        b = getb(1);
        if (b != EOF) return b;
    } while (driver_errno == -EBUSY);
    return driver_errno;
}

static Driver driver = {
    .name         = "RS232",
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

static const Driver *rs232_driver_generic(const char *dev, int baudlow) {
    i_dev = dev != NULL ? dev : DEFAULT_COMPORT;
    i_baudlow = baudlow;
    log_printf("Using %s driver on device %s", driver.name, i_dev);
    return &driver;
}

const Driver *rs232_driver(const char *dev) {
    driver.name = "RS232";
    return rs232_driver_generic(dev, 0);
}

const Driver *rs232s_driver(const char *dev) {
    driver.name = "RS232S";
    return rs232_driver_generic(dev, 1);
}
