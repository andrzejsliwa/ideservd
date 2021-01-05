/*

 compat.c - A mostly working PCLink server for IDEDOS 0.9x

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
#include "compat.h"
#include "driver.h"
#include "path.h"
#include "crc8.h"
#include "partition.h"
#include "log.h"
#include "arguments.h"
#include "buffer.h"
#include "ideservd.h"
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

#define OPEN_RONLY 0
#define OPEN_STATUS 2
#define OPEN_WONLY 4
#define OPEN_ERR 6

static unsigned long long duration(const struct timeval *start) {
    struct timeval end;
    gettimeofday(&end, NULL);
    return (end.tv_sec - start->tv_sec) * 1000000ull + end.tv_usec - start->tv_usec;
}

static int check_trailer(const Driver *driver, const char *mode, int usecrc) {
    int fr, crcerr = 0;
    if (usecrc) {
        driver->getb(0);
        crcerr = crc_get();
    }
    fr = driver->getb(0);
    if (driver->done()) return 1;
    if (fr != 0x00) {
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

int openfile_compat(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    Petscii cmd[256];
    unsigned char status = OPEN_ERR;
    unsigned char mode = 'R';
    unsigned char channel, fp;
    struct timeval start;
    Buffer *buffer;
    int r;

    channel = driver->getb(1) & 0x0f;
    buffer = &buff[channel];

    fp = 0;
    while (fp < 255) {
        r = driver->getb(0);
        if (r == EOF) break;
        cmd[fp] = r;
        if (!cmd[fp]) break;
        fp++;
    }
    cmd[fp] = 0;

    if (arguments->mode != M_ETHERNET && usecrc) {
        if (check_trailer(driver, "Open", usecrc)) return 1;
        if (r != 0) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Open: Frame error");
            return 1;
        }
    } else {
        if (driver->done()) return 1;
        if (arguments->mode != M_ETHERNET && r != 0) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Open: Frame error");
            return 1;
        }
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

    if (channel == 15) {
        buffer->mode = CM_ERR;
        commandchannel(cmd);
        status = OPEN_STATUS;
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
            seterror(ER_NO_CHANNEL, 0);
        } else if (channel > 1) {
            Directory *directory = directory_open(outpath, arguments->nameconversion, 3);
            if (directory == NULL) {
                errtochannel15(1);
                log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
                goto vege;
            }
            if (buffer_rawdir(buffer, directory)) {
                errtochannel15(1);
                log_print("Open: Out of memory");
                directory_close(directory);
            } else {
                errtochannel15(directory_close(directory));
            }
            status = OPEN_RONLY;
        } else if (partition) {
            if (buffer_partition(buffer)) {
                errtochannel15(1);
                log_print("Open: Out of memory");
            } else {
                errtochannel15(0);
            }
            status = OPEN_RONLY;
        } else {
            Directory *directory = directory_open(outpath, arguments->nameconversion, 2);

            if (directory == NULL) {
                errtochannel15(1);
                log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
                goto vege;
            }
            if (buffer_cookeddir(buffer, directory, outpart, outname)) {
                errtochannel15(1);
                log_print("Open: Out of memory");
                directory_close(directory);
            } else {
                errtochannel15(directory_close(directory));
            }
            status = OPEN_RONLY;
        }
    } else {
        Petscii name[17], type[4] = {'*', 0};
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

        if (!cmd[0]) goto vege;

        outname = resolv_path(cmd, outpath, NULL, arguments->nameconversion);
        if (outname == NULL) {
            seterror(ER_PATH_NOT_FOUND, 0); goto vege;
        }
        if (channel == 1) strcpy((char *)type, "PRG");
        convertfilename(outname, ',', name, type, &mode);

        directory = directory_open(outpath, arguments->nameconversion, 0);
        if (directory == NULL) {
            errtochannel15(1);
            log_printf("Open: Couldn't open the directory \"%s\": %s(%d)", outpath, strerror(errno), errno);
            goto vege;
        }

        while (directory_read(directory, &dirent))
        {
            if ((dirent.attrib & (A_ANY | A_CLOSED)) != (A_CLOSED | A_NORMAL)) continue;

            if (!matchname(dirent.name, name)) continue;
            if (!matchname(dirent.filetype, type)) continue;

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
            seterror(ER_MISSING_FILENAME, 0); goto vege;
        }

        if (mode == 'W') {
            int f = 0;

            while (lname[f]) {
                if (lname[f] == '*' || lname[f] == '?' || lname[f] == ':' || lname[f] == '=') {
                    seterror(ER_INVALID_FILENAME, 0); goto vege;
                }
                f++;
            }

            if (found && !overwrite) {
                seterror(ER_FILE_EXISTS, 0);
            } else {
#ifndef WIN32
                buffer->fd = open(outpath, O_CREAT | O_WRONLY | (overwrite ? O_TRUNC : O_EXCL), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#else
                buffer->fd = open(outpath, O_CREAT | O_WRONLY | (overwrite ? O_TRUNC : O_EXCL) | O_BINARY, S_IRUSR | S_IWUSR);
#endif
                if (buffer->fd < 0) {
                    errtochannel15(1);
                } else {
                    status = OPEN_WONLY;
                    buffer->file = fdopen(buffer->fd, "wb");
                    buffer->mode = CM_COMPAT;
                }
            }
        } else {
            if (!found) {
                seterror(ER_FILE_NOT_FOUND, 0);
            } else {
                switch (mode) {
                case 'R':
                    buffer->fd = open(outpath, O_RDONLY | O_BINARY, 0);
                    if (buffer->fd < 0) {
                        errtochannel15(1);
                    } else {
                        status = OPEN_RONLY;//ok
                        buffer->file = fdopen(buffer->fd, "rb");
                        buffer->mode = CM_COMPAT;
                    }
                    break;
                case 'A':
                    buffer->fd = open(outpath, O_WRONLY | O_BINARY, 0);
                    if (buffer->fd < 0) {
                        errtochannel15(1);
                    } else {
                        status = OPEN_WONLY;//ok
                        buffer->file = fdopen(buffer->fd, "ab");
                        buffer->mode = CM_COMPAT;
                    }
                    break;
                default:
                    seterror(ER_FILE_TYPE_MISMATCH, 0);
                }
            }
        }
    }
vege:
    if (arguments->verbose) {
        log_time("Open: Took", duration(&start));
    }
    driver->sendb(status);
    if (arguments->mode == M_ETHERNET) driver->sendb(status);
    return driver->flush() != 0;
}

int readfile_compat(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    unsigned char channel, status = 0;
    size_t j;
    unsigned int bytes;
    unsigned int l = 0;
    struct timeval start;
    Buffer *buffer;
    int r;

    channel = driver->getb(1) & 0x0f;
    buffer = &buff[channel];
    r = driver->getb(0);
    if (arguments->mode == M_ETHERNET) {
        bytes = r != 0 ? r : 256;
        if (driver->done()) return 1;
        if (r == EOF) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Read: Frame error");
            return 1;
        }
    } else {
        bytes = r | (driver->getb(0) << 8);
        if (bytes == 0) bytes = 65536;
        if (check_trailer(driver, "Read", usecrc)) return 1;
    }

    if (arguments->verbose) log_printf("Read #%d: %d bytes", channel, bytes);
    if (arguments->mode != M_ETHERNET) driver->sendb(0);
    if (buffer->mode != CM_DIR && buffer->mode != CM_ERR && (buffer->mode != CM_COMPAT || buffer->file == NULL)) {
        log_print((buffer->mode == CM_CLOSED) ? "Read: Channel not open" : "Read: Not readable");
    error:
        crc_clear(0);
        driver->sendb(0);
        driver->sendb(0);
        if (arguments->mode != M_ETHERNET) {
            if (usecrc) {
                driver->sendb(crc_get());
            }
            driver->sendb(0);
        }
        return driver->flush() != 0;
    }
    if (buffer->mode == CM_COMPAT) {

        if (buffer_reserve(buffer, bytes)) {
            log_print("Read: Out of memory");
            goto error;
        }
        bytes = l = fread(buffer->data, 1, bytes, buffer->file);
        if (feof(buffer->file)) status = 0x42;
        else {
            int c = fgetc(buffer->file);
            if (feof(buffer->file)) status = 0x40;
            ungetc(c, buffer->file);
        }
        j = 0;
    } else {
        size_t length = buffer->size;
        if (buffer->mode == CM_ERR) {
            buffer->data[length++] = '\r';
        }
        j = buffer->pointer;
        if (bytes == (length - j)) status = 0x40;
        else if (bytes > length - j) {
            bytes = length - j; status = 0x42;
        }
        l = bytes;
        buffer->pointer += bytes;
    }

    if (arguments->mode == M_ETHERNET) {
        unsigned int i;
        driver->sendb(bytes > 0);
        driver->sendb(bytes);
        for (i = 0; i < bytes; i++) driver->sendb(buffer->data[bytes - i - 1 + j]);
        if (bytes & 1) driver->sendb(status);
        driver->sendb(status);
        driver->sendb(status);
        if (driver->flush()) return 1;
    } else {
        crc_clear(0);
        if (arguments->verbose) gettimeofday(&start, NULL);
        driver->sendb(~bytes);
        driver->sendb(~bytes >> 8);
        driver->sendbytes(buffer->data + j, bytes);
        if (usecrc) {
            driver->sendb(crc_get());
        }
        driver->sendb(status);//log_printf("Status: %02X", status);
        if (driver->flush()) return 1;
        if (arguments->verbose) {
            log_speed("Read: Sent", l, duration(&start));
        }
    }
    if (buffer->mode == CM_ERR && buffer->size + 1 >= buffer->pointer) seterror(ER_OK, 0);
    return 0;
}

int closefile_compat(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    unsigned char channel;
    Buffer *buffer;
    struct timeval start;
    int r;

    r = driver->getb(1);
    channel = r & 0x0f;
    buffer = &buff[channel];
    if (arguments->mode != M_ETHERNET) {
        if (check_trailer(driver, "Close", usecrc)) return 1;
    } else {
        if (driver->done()) return 1;
        if (r == EOF) {
            seterror(ER_FRAME_ERROR, 1);
            log_print("Close: Frame error");
            return 1;
        }
    }

    if (arguments->verbose) {
        gettimeofday(&start, NULL);
    }
    if (arguments->verbose) log_printf("Close #%d", channel);
    if (buffer->mode == CM_COMPAT) {
        if (buffer->file != NULL) {
            fclose(buffer->file);
            buffer->file = NULL;
        }
        if (channel == 15) buffer->mode = CM_ERR;
        else {
            buffer->mode = CM_CLOSED;
            if (buffer->data != NULL) {
                buffer->capacity = 0;
                free(buffer->data);
                buffer->data = NULL;
            }
        }
    }
    if (arguments->verbose) {
        log_time("Close: Took", duration(&start));
    }

    driver->sendb(0);
    if (arguments->mode == M_ETHERNET) driver->sendb(0);
    return driver->flush() != 0;
}

int writefile_compat(const Driver *driver, Buffer *buff, const Arguments *arguments, int flags) {
    int usecrc = flags & FLAG_USE_CRC;
    unsigned char channel;
    unsigned short int bytes;
    struct timeval start, end;
    Buffer *buffer;

    channel = driver->getb(1) & 0x0f;
    buffer = &buff[channel];
    bytes = driver->getb(0);
    if (arguments->mode == M_ETHERNET) {
        driver->getb(0);
    } else {
        bytes |= driver->getb(0) << 8;
        if (check_trailer(driver, "Write", usecrc)) return 1;
    }

    if (arguments->verbose) log_printf("Write #%d: %d bytes", channel, bytes);
    if (buffer->mode == CM_ERR) {
        Petscii cmd[256];

        if (arguments->mode == M_ETHERNET) {
            driver->getbytes(cmd, bytes);
            if (driver->done()) return 1;
            driver->sendb(0);
            driver->sendb(0);
        } else {
            driver->sendb(0);
            if (driver->flush()) return 1;

            crc_clear(0);
            driver->getbytes(cmd, bytes);
            if (check_trailer(driver, "Write", usecrc)) return 1;

            driver->sendb(0);
        }
        if (driver->flush()) return 1;
        cmd[bytes - (bytes && cmd[bytes - 1] == 0x0d)] = 0;
        commandchannel(cmd);
        return 0;
    } else {
        if (buffer->mode != CM_COMPAT) {
            driver->done();
            log_print("Write: Not writeable");
        error:
            driver->sendb(2);
            if (arguments->mode == M_ETHERNET) driver->sendb(2);
            return driver->flush() != 0;
        }

        if (buffer_reserve(buffer, bytes)) {
            log_print("Write: Out of memory");
            goto error;
        }

        if (arguments->mode == M_ETHERNET) {
            driver->getbytes(buffer->data, bytes);
            if (driver->done()) return 1;
            if (fwrite(buffer->data, 1, bytes, buffer->file) == bytes && fflush(buffer->file) == 0) {
                driver->sendb(0);
                driver->sendb(0);
            } else {
                driver->sendb(2);
                driver->sendb(2);
            }
            if (driver->flush()) return 1;
        } else {
            driver->turn();
            driver->sendb(0);
            if (driver->flush()) return 1;
            crc_clear(0);

            if (arguments->verbose) gettimeofday(&start, NULL);
            driver->getbytes(buffer->data, bytes);
            if (arguments->verbose) gettimeofday(&end, NULL);

            if (check_trailer(driver, "Write", usecrc)) return 1;

            if (fwrite(buffer->data, 1, bytes, buffer->file) == bytes && fflush(buffer->file) == 0) {
                driver->sendb(0);
            } else {
                driver->sendb(2);
            }
            if (driver->flush()) return 1;
            if (arguments->verbose) {
                log_speed("Write: Received", bytes, duration(&start));
            }
        }
    }
    return 0;
}

