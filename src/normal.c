/*

 normal.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "normal.h"
#include "driver.h"
#include "path.h"
#include "crc8.h"
#include "partition.h"
#include "log.h"
#include "arguments.h"
#include "buffer.h"
#include "ideservd.h"
#ifdef __MINGW32__
#define lstat stat
#endif
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

static unsigned long long duration(const struct timeval *start) {
    struct timeval end;
    gettimeofday(&end, NULL);
    return (end.tv_sec - start->tv_sec) * 1000000ull + end.tv_usec - start->tv_usec;
}

static inline int send_trailer(const Driver *driver, const Arguments *arguments, int usecrc) {
    if (arguments->mode != M_ETHERNET) {
        if (usecrc) {
            driver->sendb(crc_get());
        }
        driver->sendb(0x5a);
    }
    return driver->flush();
}

static int check_trailer(const Driver *driver, const char *mode, int usecrc) {
    int fr, crcerr = 0;
    if (usecrc) {
        driver->getb(0);
        crcerr = crc_get();
    }
    fr = driver->getb(0);
    if (driver->done()) return 1;
    if (fr != 0x5a) {
        seterror(ER_FRAME_ERROR, 1);
        log_printf("%s: Frame error", mode);
        return 1;
    }
    if (crcerr) {
        seterror(ER_CRC_ERROR, 1);
        log_printf("%s: CRC error", mode);
        return 1;
    }
    return 0;
}

int statuserror(const Driver *driver, const char *errorbuff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    Petscii cmd[256];
    struct timeval start;
    unsigned char fp;
    int r;

    driver->getb(1);
    fp = 0;
    while (fp < 255) {
        r = driver->getb(0);
        if (r == EOF) break;
        cmd[fp] = r;
        if (!cmd[fp]) break;
        fp++;
    }
    cmd[fp] = 0;

    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Status", usecrc)) return 1;
        if (r != 0) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Status: Frame error");
            return 1;
        }
    } else {
        if (driver->done()) return 1;
    }

    if (arguments->verbose) {
        gettimeofday(&start, NULL);
    }

    if (arguments->debug) {
        log_print("Status:");
        log_hex(cmd);
    }
    commandchannel(cmd);
    if (arguments->verbose) {
        log_time("Status: Took", duration(&start));
    }

    crc_clear(0);
    fp = strlen(errorbuff);
    driver->sendb(fp);
    if (arguments->mode == M_ETHERNET) driver->sendb(fp);
    driver->sendbytes((const unsigned char *)errorbuff, fp);
    return send_trailer(driver, arguments, usecrc) != 0;
}

int openfile(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    Petscii cmd[256];
    struct timeval start;
    Errorcode status = ER_OK;
    unsigned char mode = 'R';
    unsigned char channel, fp;
    unsigned int length = 0;
    Buffer *buffer;
    int r;

    channel = driver->getb(1) & 0x0f;
    buffer = &buff[channel];

    fp = 0;
    while (fp < 255) {
        r = driver->getb(0);
        if (r == EOF) break;
        cmd[fp] = r;
        if (!r) break;
        fp++;
    }
    cmd[fp] = 0;

    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Open", usecrc)) return 1;
        if (r != 0) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Open: Frame error");
            return 1;
        }
    } else {
        if (driver->done()) return 1;
    }

    if (arguments->verbose) {
        gettimeofday(&start, NULL);
    }

    if (arguments->debug) {
        log_print("Open:");
        log_hex(cmd);
    }
    if (buffer->file != NULL) {
        fclose(buffer->file);
        buffer->file = NULL;
    }
    buffer->mode = CM_CLOSED;
    buffer->filepos = 0;

    if (channel == 15) {
        buffer->mode = CM_ERR;
    } else if (cmd[0] == '$') {
        int partition;
        char outpath[1000];
        const Petscii *outname;
        partition_t outpart = 0;

        buffer->mode = CM_DIR;
        buffer->size = buffer->pointer = 0;
        partition = !strncmp((char *)&cmd[1], "=P", 2);

        outname = resolv_path(cmd + 1, outpath, &outpart, arguments->nameconversion);
        if (arguments->verbose) log_printf("Open #%d: \"$%s\"", channel, outpath);

        if (channel == 1) {
            status = ER_NO_CHANNEL; goto vege;
        } else if (channel > 1) {
            Directory *directory = directory_open(outpath, arguments->nameconversion, 3);
            if (directory == NULL) {
                status = errtochannel15(1);
                log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
                goto vege;
            }
            if (buffer_rawdir(buffer, directory)) {
                buffer->size &= ~511;
                status = ER_READ_ERROR;
                log_print("Open: Out of memory");
                directory_close(directory);
            } else {
                status = errtochannel15(directory_close(directory));
            }
        } else if (partition) {
            if (buffer_partition(buffer)) {
                buffer->size &= ~511;
                status = ER_READ_ERROR;
                log_print("Open: Out of memory");
            } else {
                status = errtochannel15(0);
            }
        } else {
            Directory *directory = directory_open(outpath, arguments->nameconversion, 2);

            if (directory == NULL) {
                status = errtochannel15(1);
                log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
                goto vege;
            }
            if (buffer_cookeddir(buffer, directory, outpart, outname)) {
                buffer->size &= ~511;
                status = ER_READ_ERROR;
                log_print("Open: Out of memory");
                directory_close(directory);
            } else {
                status = errtochannel15(directory_close(directory));
            }
        }
        if ((buffer->size & 511) != 0) {
            unsigned int padding = (-buffer->size) & 511;
            if (buffer_reserve(buffer, buffer->size + padding)) {
                buffer->size &= ~511;
                status = ER_READ_ERROR;
                log_print("Open: Out of memory");
            } else {
                memset(buffer->data + buffer->size, 0, padding);
            }
        }
        length = buffer->size;
    } else {
        Petscii name[17], type[4]={'*',0};
        const Petscii *outname;
        unsigned char overwrite;
        char lname[1000], outpath[1020];
        int found = 0;
        int b;
        Directory_entry dirent;
        Directory *directory;

        overwrite = 0;
        for (b = 0; cmd[b]; b++) {
            if (cmd[b] != ':') continue;
            if (cmd[0] == '@') overwrite = 1;
            break;
        }

        outname = resolv_path(cmd, outpath, NULL, arguments->nameconversion);
        if (outname == NULL) {
            status = ER_PATH_NOT_FOUND; goto vege;
        }
        if (channel == 1) strcpy((char *)type, "PRG");
        convertfilename(outname, ',', name, type, &mode);

        directory = directory_open(outpath, arguments->nameconversion, 0);
        if (directory == NULL) {
            status = errtochannel15(1);
            log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
            goto vege;
        }

        while (directory_read(directory, &dirent))
        {
            struct stat buf;

            if ((dirent.attrib & (A_ANY | A_CLOSED)) != (A_CLOSED | A_NORMAL)) continue;

            if (!matchname(dirent.name, name)) continue;
            if (!matchname(dirent.filetype, type)) continue;

            if (lstat(directory_path(directory), &buf)) continue;
            length = buf.st_size;

            strncpy(lname, directory_filename(directory), 999);

            found = 1;
            break;
        }
        directory_close(directory);

        if (!found) convertc64name(lname, name, type, arguments->nameconversion);

        if (channel == 0) mode = 'R';
        if (channel == 1) mode = 'W';

        if (outpath[0]) strcat(outpath, "/");
        strcat(outpath, lname);

        if (arguments->verbose) log_printf("Open #%d: \"%s\" %c", channel, outpath, mode);

        if (!name[0]) {
            status = ER_MISSING_FILENAME; goto vege;
        }

        if (mode == 'W') {
            int f = 0;
            length = 0;

            while (lname[f]) {
                if (lname[f] == '*' || lname[f] == '?' || lname[f] == ':' || lname[f] == '=') {
                    status = ER_INVALID_FILENAME; goto vege;
                }
                f++;
            }

            if (found && !overwrite) {
                status = ER_FILE_EXISTS;
            } else {
#ifndef WIN32
                buffer->fd = open(outpath, O_CREAT | O_RDWR | (overwrite ? O_TRUNC : O_EXCL), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#else
                buffer->fd = open(outpath, O_CREAT | O_RDWR | (overwrite ? O_TRUNC : O_EXCL) | O_BINARY, S_IRUSR | S_IWUSR);
#endif
                if (buffer->fd < 0) status = errtochannel15(1); else {
                    status = ER_OK;
                    buffer->file = fdopen(buffer->fd, "wb+");
                    buffer->mode = CM_FILE;
                    buffer->filesize = 0;
                }
            }
        } else {
            if (!found) {
                status = ER_FILE_NOT_FOUND;
            } else {
                if (mode == 'R') {
                    buffer->fd = open(outpath, O_RDONLY | O_BINARY, 0);
                    if (buffer->fd < 0) status = errtochannel15(1); else {
                        status = ER_OK;
                        buffer->file = fdopen(buffer->fd, "rb");
                        buffer->mode = CM_FILE;
                        buffer->filesize = length;
                    }
                }
                if (mode == 'A' || mode == 'M') {
                    buffer->fd = open(outpath, O_RDWR | O_BINARY, 0);
                    if (buffer->fd < 0) status = errtochannel15(1); else {
                        status = ER_OK;
                        buffer->file = fdopen(buffer->fd, "rb+");
                        buffer->mode = CM_FILE;
                        buffer->filesize = length;
                    }
                }
            }
        }
    }
vege:
    if (arguments->verbose) {
        log_time((channel == 15) ? "Status: Took" : "Open: Took", duration(&start));
    }
    {
        unsigned char buf[6];
        if (arguments->mode == M_ETHERNET) {
            buf[0] = length;
            buf[1] = length >> 8;
            buf[2] = length >> 16;
            buf[3] = length >> 24;
            buf[4] = mode;
            buf[5] = status | 0x80;
        } else {
            crc_clear(0);
            buf[0] = status | 0x80;
            buf[1] = mode;
            buf[2] = length;
            buf[3] = length >> 8;
            buf[4] = length >> 16;
            buf[5] = length >> 24;
        }
        driver->sendbytes(buf, sizeof buf);
        return send_trailer(driver, arguments, usecrc) != 0;
    }
}

int closefile(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    unsigned char channel;
    unsigned int length;
    struct timeval start;
    Buffer *buffer;
    unsigned char buf[5];

    driver->getbytes(buf, sizeof buf);
    channel = buf[0] & 0x0f;
    length = (((((buf[4] << 8) | buf[3]) << 8) | buf[2]) << 8) | buf[1];
    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Close", usecrc)) return 1;
    } else {
        if (driver->done()) return 1;
    }
    buffer = &buff[channel];

    if (arguments->verbose) {
        gettimeofday(&start, NULL);
    }

    if (arguments->verbose) log_printf("Close #%d: %d byte(s)", channel, length);
    if (channel == 15) buffer->mode = CM_ERR;
    else {
        if (buffer->file != NULL) {
            if (buffer->mode == CM_FILE && length >= buffer->filesize) {
                fflush(buffer->file);
                if (ftruncate(buffer->fd, length)) {
                    log_printf("Close: Couldn't truncate: %s(%d)", strerror(errno), errno);
                }
            }
            fclose(buffer->file);
            buffer->file = NULL;
        }
        buffer->mode = CM_CLOSED;
        if (buffer->data != NULL) {
            buffer->capacity = 0;
            free(buffer->data);
            buffer->data = NULL;
        }
    }
    if (arguments->verbose) {
        log_time("Close: Took", duration(&start));
    }
    crc_clear(0);
    driver->sendb(0x80 | ER_OK);
    if (arguments->mode == M_ETHERNET) driver->sendb(0x80 | ER_OK);
    return send_trailer(driver, arguments, usecrc) != 0;
}

int readfile(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    unsigned char channel, sectors;
    struct timeval start;
    unsigned int l = 0;
    unsigned int address;
    Buffer *buffer;
    unsigned char buf[5];

    driver->getbytes(buf, sizeof buf);
    channel = buf[0] & 0x0f;
    address = (((buf[3] << 8) | buf[2]) << 8) | buf[1];
    sectors = buf[4];
    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Read", usecrc)) return 1;
    } else {
        if (driver->done()) return 1;
    }
    buffer = &buff[channel];

    if (arguments->verbose) log_printf("Read #%d: %06x %d sector(s)", channel, address, sectors);
    if (sectors > 128 || sectors == 0 || (arguments->mode == M_ETHERNET && sectors > 2)) {
        log_print("Read: Invalid sector count");
        goto error;
    }
    if (buffer->mode != CM_DIR && buffer->mode != CM_ERR && (buffer->mode != CM_FILE || buffer->file == NULL)) {
        log_print((buffer->mode == CM_CLOSED) ? "Read: Channel not open" : "Read: Not readable");
        crc_clear(0);
        driver->sendb(0x80 | ER_NO_CHANNEL);
        if (arguments->mode == M_ETHERNET) driver->sendb(0x80 | ER_NO_CHANNEL);
        return send_trailer(driver, arguments, usecrc) != 0;
    }

    if (buffer->mode == CM_FILE) {
        if (buffer_reserve(buffer, (arguments->mode == M_ETHERNET) ? sectors * 512 : 512)) {
            log_print("Read: Out of memory");
            goto error;
        }
        if (buffer->filepos != address && fseek(buffer->file, address << 8, SEEK_SET)) {
            buffer->filepos = -1;
            log_printf("Read: Couldn't seek: %s(%d)", strerror(errno), errno);
        error:
            crc_clear(0);
            driver->sendb(0x80 | ER_READ_ERROR);
            if (arguments->mode == M_ETHERNET) driver->sendb(0x80 | ER_READ_ERROR);
            return send_trailer(driver, arguments, usecrc) != 0;
        }
        buffer->filepos = address;
        clearerr(buffer->file);
    }

    if (arguments->mode == M_ETHERNET) {
        const unsigned char *data;
        if (buffer->mode == CM_FILE) {
            size_t itt = fread(buffer->data, 1, sectors * 512, buffer->file);
            buffer->filepos += ((itt + 511) >> 9) * 2;
            if (ferror(buffer->file)) {
                buffer->filepos = -1;
                log_printf("Read: Couldn't read: %s(%d)", strerror(errno), errno);
                driver->sendb(0x80 | ER_READ_ERROR);
                driver->sendb(0x80 | ER_READ_ERROR);
                if (send_trailer(driver, arguments, usecrc)) return 1;
            }
            if (itt < sectors * 512) memset(buffer->data + itt, 0, sectors * 512 - itt);
            data = buffer->data;
        } else {
            data = buffer->data + buffer->pointer;
        }
        driver->sendbytes(data, sectors * 512);
        driver->sendb(0x80 | ER_OK);
        driver->sendb(0x80 | ER_OK);
        if (send_trailer(driver, arguments, usecrc)) return 1;
        buffer->pointer += sectors * 512;
    } else {
        crc_clear(0);
        driver->sendb(0x80 | ER_OK);
        if (arguments->mode == M_RS232 || arguments->mode == M_RS232S) {
            if (send_trailer(driver, arguments, usecrc)) return 1;
        } else {
            if (usecrc) {
                driver->sendb(crc_get());
            }
            driver->sendb(0x5a);
        }
        if (arguments->verbose) {
            gettimeofday(&start, NULL);
        }
        while (sectors) {
            int err = 0;
            const unsigned char *data;
            if (buffer->mode == CM_FILE) {
                size_t itt = fread(buffer->data, 1, 512, buffer->file);
                buffer->filepos += ((itt + 511) >> 9) * 2;
                err = ferror(buffer->file);
                if (err) {
                    buffer->filepos = -1;
                    log_printf("Read: Couldn't read: %s(%d)", strerror(errno), errno);
                }
                if (itt < 512) memset(buffer->data + itt, 0, 512 - itt);
                data = buffer->data;
            } else {
                data = buffer->data + buffer->pointer;
            }
            if (arguments->mode == M_RS232 || arguments->mode == M_RS232S) {
                int fr = driver->getb(1);
                if (driver->done()) return 1;
                if (fr != 0x5a) {
                    seterror(ER_FRAME_ERROR, 1);
                    log_print("Read: Frame error");
                    return 1;
                }
            }
            crc_clear(0);
            driver->sendbytes(data, 512);
            driver->sendb(err ? 0x80 | ER_READ_ERROR : 0x80 | ER_OK);
            if (arguments->mode == M_RS232 || arguments->mode == M_RS232S) {
                if (send_trailer(driver, arguments, usecrc)) return 1;
            } else {
                if (usecrc) {
                    driver->sendb(crc_get());
                }
                driver->sendb(0x5a);
            }
            if (err) break;
            buffer->pointer += 512;
            sectors--; l += 512;
        }
        if (arguments->mode != M_RS232 && arguments->mode != M_RS232S) {
            if (driver->flush()) return 1;
        }
        if (arguments->verbose) {
            log_speed("Read: Sent", l, duration(&start));
        }
    }
    return 0;
}

int writefile(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    int usepadding = flags & FLAG_USE_PADDING;
    unsigned char channel, sectors;
    struct timeval start;
    unsigned int l = 0;
    unsigned int address;
    Buffer *buffer;
    int fr;
    unsigned char buf[5];

    driver->getbytes(buf, sizeof buf);
    channel = buf[0] & 0x0f;
    address = (((buf[3] << 8) | buf[2]) << 8) | buf[1];
    sectors = buf[4];
    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Write", usecrc)) return 1;
    } else {
        if (driver->done()) return 1;
    }
    buffer = &buff[channel];

    if (arguments->verbose) log_printf("Write #%d: %06x %d sector(s)", channel, address, sectors);
    if (sectors > 128 || sectors == 0 || (arguments->mode == M_ETHERNET && sectors > 2)) {
        log_print("Write: Invalid sector count");
        goto error;
    }
    if (buffer->mode != CM_FILE || buffer->file == NULL) {
        log_print("Write: Not writeable");
    error:
        crc_clear(0);
        driver->sendb(0x80 | ER_WRITE_ERROR);
        if (arguments->mode == M_ETHERNET) driver->sendb(0x80 | ER_WRITE_ERROR);
        return send_trailer(driver, arguments, usecrc) != 0;
    }

    if (buffer_reserve(buffer, (arguments->mode == M_ETHERNET) ? 512 * sectors : 514)) {
        log_print("Write: Out of memory");
        goto error;
    }
    if (buffer->filepos != address && fseek(buffer->file, address << 8, SEEK_SET)) {
        buffer->filepos = -1;
        log_printf("Write: Couldn't seek: %s(%d)", strerror(errno), errno);
        goto error;
    }
    buffer->filepos = address;
    if (arguments->mode == M_ETHERNET) {
        driver->getbytes(buffer->data, 512 * sectors);
        if (driver->done()) return 1;
        if (fwrite(buffer->data, 1, 512 * sectors, buffer->file) == 512 * sectors && fflush(buffer->file) == 0) {
            buffer->filepos += sectors * 2;
            driver->sendb(0x80 | ER_OK);
            driver->sendb(0x80 | ER_OK);
        } else {
            buffer->filepos = -1;
            log_printf("Write: Couldn't write: %s(%d)", strerror(errno), errno);
            driver->sendb(0x80 | ER_WRITE_ERROR);
            driver->sendb(0x80 | ER_WRITE_ERROR);
        }
        if (send_trailer(driver, arguments, usecrc)) return 1;
    } else {
        int err = 0;
        crc_clear(0);
        driver->turn();
        driver->sendb(0x80 | ER_OK);
        if (send_trailer(driver, arguments, usecrc)) return 1;
        if (arguments->verbose) {
            gettimeofday(&start, NULL);
        }
        while (sectors) {
            int crcerr = 0;
            crc_clear(0);
            driver->getbytes(buffer->data, usepadding ? 514 : 513);
            if (usecrc) {
                crcerr = crc_get();
                fr = driver->getb(1);
            } else {
                fr = buffer->data[usepadding ? 513 : 512];
            }
            if (driver->done()) return 1;
            if (fr != 0x5a) {
                seterror(ER_FRAME_ERROR, 1);
                log_print("Write: Frame error");
                return 1;
            }
            if (crcerr) {
                seterror(ER_CRC_ERROR, 1);
                log_print("Write: CRC error");
                return 1;
            }
            if (err == 0 && fwrite(buffer->data, 1, 512, buffer->file) != 512) err = errno;
            buffer->filepos += 2;
            sectors--; l += 512;
            if (sectors == 0 || arguments->mode == M_RS232 || arguments->mode == M_RS232S) {
                if (err == 0 && fflush(buffer->file) != 0) err = errno;
                if (err != 0) {
                    buffer->filepos = -1;
                    log_printf("Write: Couldn't write: %s(%d)", strerror(err), err);
                }
                crc_clear(0);
                driver->sendb(err ? 0x80 | ER_WRITE_ERROR : 0x80 | ER_OK);
                if (send_trailer(driver, arguments, usecrc)) return 1;
                if (err) break;
            }
        }
        if (arguments->verbose) {
            log_speed("Write: Received", l, duration(&start));
        }
    }
    return 0;
}

