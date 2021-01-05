/*

 ideservd.c - A mostly working PCLink server for IDEDOS 0.9x

 Written by
  Kajtar Zsolt <soci@c64.rulez.org>
 Win32 port by
  Josef Soucek <josef.soucek@ide64.org>
 Serial support by
  Borsanyi Attila <bigfoot@axelero.hu>

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

#include "ideservd.h"
#include <errno.h>
#ifndef WIN32           //*NIX
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#else                   //WIN32
#include <windows.h>
#include "resource.h"
#endif                  //ALL
#include <signal.h>
#include <unistd.h>
#ifdef __DJGPP__
#define chroot(a) 0
#endif
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <locale.h>
#include "path.h"
#include "partition.h"
#include "driver.h"
#include "arguments.h"
#ifndef __DJGPP__
#include "usb.h"
#include "eth.h"
#include "vice.h"
#endif
#include "rs232.h"
#ifndef OSX
#include "x1541.h"
#include "pc64.h"
#endif

#include "crc8.h"
#include "compat.h"
#include "normal.h"
#include "log.h"
#include "message.h"
#include "buffer.h"
#ifdef __MINGW32__
#define mkdir(a, b) mkdir (a)
#endif

static const Driver *driver;

static Arguments arguments;
static Buffer buff[16];

#ifdef WIN32
/* Prototypes */
static long WINAPI PCLinkLoop(long);
#else
#ifndef  __DJGPP__
#define FORKING
static int pipefd = -1;
#endif
#endif

static void terminate(int x) {
    (void)x;
    int b;
    if (driver != NULL) driver->shutdown();
    for (b = 0; b < 16; b++) {
        Buffer *buffer = &buff[b];
        if (buffer->file != NULL) fclose(buffer->file);
        if (buffer->data != NULL) free(buffer->data);
    }
    for (b = 0; b < 256; b++) partition_set_path(b, NULL);
#ifdef FORKING
    if (pipefd >= 0) close(pipefd);
#endif
    log_print("Terminated");
    exit(0);
}

void seterror(Errorcode c, int i1) {
    const char *msg;

    switch (c) {
    case ER_OK: msg = "OK"; break;
    case ER_FILES_SCRATCHED: msg = "FILES SCRATCHED"; break;
    case ER_PARTITION_SELECTED: msg = "PARTITION SELECTED"; break;
    case ER_READ_ERROR: if (i1 == 0) msg = "READ ERROR"; else msg = "CRC ERROR"; i1 = 0; break;
    case ER_WRITE_PROTECT_ON: msg = "WRITE PROTECT ON"; break;
    case ER_ACCESS_DENIED: msg = "ACCESS DENIED"; break;
    case ER_UNKNOWN_COMMAND: msg = "UNKNOWN COMMAND"; break;
    case ER_SYNTAX_ERROR:
    case ER_INVALID_FILENAME:
    case ER_MISSING_FILENAME: msg = "SYNTAX ERROR"; break;
    case ER_PATH_NOT_FOUND: msg = "PATH NOT FOUND"; break;
    case ER_FRAME_ERROR: msg = "FRAME ERROR"; break;
    case ER_CRC_ERROR: msg = "CRC ERROR"; break;
    case ER_FILE_NOT_FOUND: msg = "FILE NOT FOUND"; break;
    case ER_FILE_EXISTS: msg = "FILE EXISTS"; break;
    case ER_FILE_TYPE_MISMATCH: msg = "FILE TYPE MISMATCH"; break;
    case ER_NO_CHANNEL: msg = "NO CHANNEL"; break;
    case ER_PARTITION_FULL: msg = "PARTITION FULL"; break;
    case ER_DOS_VERSION: msg = "IDESERV " VERSION2 " PCLINK"; break;
    case ER_SELECTED_PARTITION_ILLEGAL: msg = "SELECTED PARTITION ILLEGAL"; break;
    default: msg = "UNKNOWN ERROR"; break;
    }
    buff[15].size = sprintf((char *)buff[15].data, "%02d, %s,%03d,000,000,000", c, msg, i1);
    buff[15].pointer = 0;
}

Errorcode errtochannel15(int i) {
    if (i)
        switch (errno) {
        case EISDIR:
        case ENOTDIR:
        case ENOENT: seterror(ER_FILE_NOT_FOUND, 0); return ER_FILE_TYPE_MISMATCH;
        case EEXIST: seterror(ER_FILE_EXISTS, 0); return ER_FILE_EXISTS;
        case EPERM:
        case EACCES: seterror(ER_ACCESS_DENIED, 0); return ER_ACCESS_DENIED;
        case ENOTEMPTY: seterror(ER_FILES_SCRATCHED, 0); return ER_FILES_SCRATCHED;
        case ENOSPC: seterror(ER_PARTITION_FULL, 0); return ER_PARTITION_FULL;
        case EROFS: seterror(ER_WRITE_PROTECT_ON, 0); return ER_WRITE_PROTECT_ON;
        case EIO: seterror(ER_READ_ERROR, 0); return ER_READ_ERROR;
        default: seterror(ER_UNKNOWN_ERROR, errno); return ER_OK;
        }
    seterror(ER_OK, 0);
    return ER_OK;
}

#ifdef FORKING
static int partition_select2(partition_t partition) {
    if (pipefd >= 0) {
        unsigned char msg[3];
        msg[0] = 2;
        msg[1] = partition;
        msg[2] = 0;
        write(pipefd, msg, sizeof msg);
    }
    return partition_select(partition);
}

static void partition_set_path2(partition_t partition, const char *path) {
    if (pipefd >= 0) {
        unsigned char msg[3];
        msg[0] = 1;
        msg[1] = partition;
        msg[2] = strlen(path);
        write(pipefd, msg, sizeof msg);
        if (msg[2] != 0) write(pipefd, path, msg[2]);
    }
    partition_set_path(partition, path);
}
#else
#define partition_set_path2 partition_set_path
#define partition_select2 partition_select
#endif

static void do_chdir(const Petscii *cmd) {
    Petscii name[17];
    const Petscii *outname;
    char outpath[1020];
    int found = 0;
    partition_t outpart = 0;
    Directory_entry dirent;
    Directory *directory;

    if (!strchr((char *)cmd, ':')) {
        if (!strcmp((char *)cmd, "_")) cmd = (Petscii *)":_";
        else if (cmd[0] != '/') {
            seterror(ER_SYNTAX_ERROR, 0); goto vege3;
        } else cmd++;
    }

    outname = resolv_path(cmd, outpath, &outpart, arguments.nameconversion);
    if (outname == NULL) goto vege2;

    convertfilename(outname, ',', name, NULL, NULL);

    if ((name[0] == '_' && name[1] == 0) || (name[0] == '.' && name[1] == '.' && name[2] == 0)) {
        char *o = strrchr(outpath, '/');
        if (o == NULL) o = outpath;
        *o = 0;
        found = 1;
        goto vege2;
    }
    if (name[0] == 0 || (name[0] == '.' && name[1] == 0)) {
        found = 1;
        goto vege2;
    }

    directory = directory_open(outpath, arguments.nameconversion, 0);
    if (directory == NULL) {
        errtochannel15(1);
        goto vege3;
    }

    while (directory_read(directory, &dirent)) {
        if ((dirent.attrib & A_ANY) != A_DIR) continue;

        if (!matchname(dirent.name, name)) continue;

        strcpy(outpath, directory_path(directory));
        found = 1;
        break;
    }
    directory_close(directory);
vege2:
    if (found) {
        partition_set_path2(outpart, outpath);
        seterror(ER_OK, 0);
    } else seterror(ER_PATH_NOT_FOUND, 0);
vege3:
    if (arguments.verbose) log_printf("Command: Change directory \"%s\"", outpath[0] ? outpath : "/");
}

static void do_mkdir(const Petscii *cmd) {
    char outpath[1020], lname[1000];
    int found = 0;
    Petscii name[17];
    const Petscii *outname;
    Directory_entry dirent;
    Directory *directory;

    if (!strchr((char *)cmd, ':')) {
        seterror(ER_SYNTAX_ERROR, 0); outpath[0] = 0; goto vege2;
    }

    outname = resolv_path(cmd, outpath, NULL, arguments.nameconversion);
    if (outname == NULL) {
        seterror(ER_PATH_NOT_FOUND, 0); goto vege2;
    }

    convertfilename(outname, ',', name, NULL, NULL);

    if (!name[0]) {
        seterror(ER_MISSING_FILENAME, 0);
        goto vege2;
    }

    directory = directory_open(outpath, arguments.nameconversion, 0);
    if (directory == NULL) {
        errtochannel15(1);
        goto vege2;
    }

    while (directory_read(directory, &dirent))
    {
        if ((dirent.attrib & A_ANY) != A_DIR) continue;

        if (!matchname(dirent.name, name)) continue;

        strncpy(lname, directory_filename(directory), 999);

        found = 1;
        break;
    }
    directory_close(directory);

    if (!found) {
        mbstate_t ps;
        int f, f2;
        memset(&ps, 0, sizeof ps);
        for (f = f2 = 0; name[f] && f < 16; f++) {
            f2 += c64toascii(lname + f2, name[f], &ps);
        }
        lname[f2] = 0;
    }

    if (outpath[0]) strcat(outpath, "/");
    strcat(outpath, lname);

    if (found)
        seterror(ER_FILE_EXISTS, 0);
    else {
        int f = 0;

        while (lname[f]) {
            if (lname[f] == '*' || lname[f] == '?' || lname[f] == ':' || lname[f] == '=') {
                seterror(ER_INVALID_FILENAME, 0); goto vege2;
            }
            f++;
        }
        errtochannel15(mkdir(outpath, 0777));
    }
vege2:
    if (arguments.verbose) log_printf("Command: Make directory \"%s\"", outpath[0] ? outpath : "/");
}

static void do_scratch(const Petscii *cmd, int smode) {
    char outpath[1000];
    int found = 0;
    Petscii name[17], type[4] = {'*', 0};
    const Petscii *outname;
    Directory_entry dirent;
    Directory *directory;

    if (!strchr((char *)cmd, ':')) {
        seterror(ER_SYNTAX_ERROR, 0); outpath[0] = 0; goto vege;
    }

    outname = resolv_path(cmd, outpath, NULL, arguments.nameconversion);
    if (outname == NULL) {
        seterror(ER_PATH_NOT_FOUND, 0); goto vege;
    }

    convertfilename(outname, '=', name, type, NULL);

    if (!name[0]) {
        seterror(ER_MISSING_FILENAME, 0);
        goto vege;
    }

    directory = directory_open(outpath, arguments.nameconversion, 0);
    if (directory == NULL) {
        errtochannel15(1);
        goto vege;
    }

    while (directory_read(directory, &dirent))
    {
        const char *path;
        if (smode) {
            if ((dirent.attrib & (A_ANY | A_CLOSED)) != (A_DIR | A_CLOSED)) continue;
        } else {
            if ((dirent.attrib & A_ANY) == A_DIR) continue;
        }

        if (!(dirent.attrib & A_DELETEABLE)) continue;

        if (!matchname(dirent.name, name)) continue;
        if (!smode) {
            if (!matchname(dirent.filetype, type)) continue;
        }

        path = directory_path(directory);
        found = 1;
        if (smode) {
            if (arguments.verbose) log_printf("Command: Remove directory \"%s\"", path);
            errtochannel15(rmdir(path));
        } else {
            errtochannel15(unlink(path));
            if (arguments.verbose) log_printf("Command: Remove \"%s\"", path);
        }
    }
    directory_close(directory);
    if (!found) seterror(ER_FILES_SCRATCHED, 0);
vege:
    if (!found) {
        if (smode) {
            if (arguments.verbose) log_printf("Command: Remove directory \"%s\"", outpath);
        } else {
            if (arguments.verbose) log_printf("Command: Remove \"%s\"", outpath);
        }
    }
}

void commandchannel(const Petscii *s) {
    if (s[0] == 'C' && s[1] == 'D') {
        do_chdir(s + 2);
    } else if (s[0] == 'M' && s[1] == 'D') {
        do_mkdir(s);
    } else if (s[0] == 'R' && s[1] == 'D') {
        do_scratch(s, 1);
    } else if (s[0] == 'S' && s[1] != '-') {
        do_scratch(s, 0);
    } else if (s[0] == 'C' && s[1] == 0xd0) {
        if (arguments.verbose) log_printf("Command: Change to partition %d", s[2]);
        seterror(partition_select2(s[2]) ? ER_SELECTED_PARTITION_ILLEGAL : ER_PARTITION_SELECTED, s[2]);
    } else if (s[0] == 'C' && s[1] == 'P') {
        partition_t part = 0;
        int i = 2;
        if (!s[2]) part = partition_get_current(); else {
            while ((s[i] ^ 0x30) < 10) part = part * 10 + (s[i++] ^ 0x30);
        }
        if (arguments.verbose) log_printf("Command: Change to partition %d", part);
        seterror(partition_select2(part) ? ER_SELECTED_PARTITION_ILLEGAL : ER_PARTITION_SELECTED, part);
    } else if (s[0] == 'U' && ((s[1] & 0x0f) == 9 || (s[1] & 0x0f) == 10)) {
        seterror(ER_DOS_VERSION, 0);
        if (arguments.verbose) log_print("Command: Identify device");
    } else if (s[0] == 'I') {
        seterror(ER_OK, 0);
        if (arguments.verbose) log_print("Command: Initialize");
    } else if (s[0] == 'V') {
        seterror(ER_OK, 0);
        if (arguments.verbose) log_print("Command: Validate");
    } else if (s[0] == 'M' && s[1] == '-' && s[2] == 'R') {
        int i, j, k;
        j = s[5]; if (j > 50) j = 50;
        k = s[3];
        for (i = 0; i < j; i++) buff[15].data[i] = "IDE64 CARTRIDGE "[k++ & 15];
        buff[15].data[i] = 0;
        if (arguments.verbose) log_printf("Command: Memory read $%04x %02x", s[3] | (s[4] << 8), s[5]);
    } else if (s[0]) {
        seterror(ER_UNKNOWN_COMMAND, 0);
    }
}

static void init(int argc, char *argv[]) {
#ifndef WIN32
    struct passwd *user2 = NULL;
    struct group *group2 = NULL;
    int lastfail = 0;
#endif

    setlocale(LC_CTYPE, "");

    testarg(&arguments, argc, argv);
    partition_create(1, (Petscii *)"PARTITION 1");
    partition_select(1);

    log_open(arguments.log);
    log_print("ideservd " VERSION);

    arguments.mode = M_NONE;
    if (arguments.mode_name) {
        const struct {
            const char *name;
            enum e_modes mode;
        } *i, modes[] = {
#ifdef _X1541_H
            {"x1541", M_X1541}, {"xe1541", M_XE1541}, {"xm1541", M_XM1541}, {"xa1541", M_XA1541},
#endif
#ifdef _PC64_H
            {"pc64", M_PC64}, {"pc64s", M_PC64S},
#endif
#ifdef _RS232_H
            {"rs232", M_RS232}, {"rs232s", M_RS232S},
#endif
#ifdef _USB_H
            {"usb", M_USB},
#endif
#ifdef _VICE_H
            {"vice", M_VICE},
#endif
#ifdef _ETH_H
            {"eth", M_ETHERNET},
#endif
            {NULL, M_NONE}
        };
        i = modes;
        while (i->name) {
            if (!strcasecmp(arguments.mode_name, i->name)) {
                arguments.mode = i->mode;
                break;
            }
            i++;
        }
    } else {
#ifdef _USB_H
        arguments.mode = M_USB;
#else
        arguments.mode = M_X1541;
#endif
    }

    switch (arguments.mode) {
#ifdef _X1541_H
    case M_X1541:
        driver = x1541_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
    case M_XE1541:
        driver = xe1541_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
    case M_XM1541:
        driver = xm1541_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
    case M_XA1541:
        driver = xa1541_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
#endif
#ifdef _PC64_H
    case M_PC64:
        driver = pc64_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
    case M_PC64S:
        driver = pc64s_driver(arguments.device, arguments.lptport, arguments.hog);
        break;
#endif
#ifdef _RS232_H
    case M_RS232:
        driver = rs232_driver(arguments.device);
        break;
    case M_RS232S:
        driver = rs232s_driver(arguments.device);
        break;
#endif
#ifdef _USB_H
    case M_USB:
        driver = usb_driver(arguments.device);
        break;
#endif
#ifdef _VICE_H
    case M_VICE:
        driver = vice_driver(arguments.sin_addr, arguments.network);
        break;
#endif
#ifdef _ETH_H
    case M_ETHERNET:
        driver = eth_driver(arguments.sin_addr, arguments.network);
        break;
#endif
    default:
        message("No driver for unknown mode \"%s\"\n", arguments.mode_name);
        exit(EXIT_FAILURE);
    }

#ifdef WIN32
    if (chdir(arguments.root ? arguments.root : "/cygdrive")) {
        if (arguments.root != NULL) {
            message("Changing dir to \"%s\" failed: %s(%d)\n", arguments.root, strerror(errno), errno);
            exit(EXIT_FAILURE);
        } else {
            log_printf("Changing dir to \"/cygdrive\" failed: %s(%d)", strerror(errno), errno);
        }
    }
#else
#ifndef  __DJGPP__
    if (arguments.background) {
        pid_t pid = fork();
        if (pid == -1) {
            message("Could not fork to background: %s(%d)\n", strerror(errno), errno);
            exit(EXIT_FAILURE);
        }
        if (pid != 0) {
            message("Forking into background\n");
            exit(EXIT_SUCCESS);
        }
    }
    for (;;) {
        pid_t pid;
        int pipefds[2];
        if (pipe(pipefds) == -1) {
            pipefds[0] = -1;
            pipefds[1] = -1;
        }
        pid = fork();
        if (pid == 0) {
            if (pipefds[0] >= 0) close(pipefds[0]);
            pipefd = pipefds[1];
            break;
        }
        if (pipefds[1] >= 0) close(pipefds[1]);
        usleep(1000000);
        if (pid != -1) {
            int status;
            if (pipefds[0] >= 0) {
                unsigned char msg[3];
                while (read(pipefds[0], &msg, sizeof msg) == sizeof msg) {
                    if (msg[0] == 1) {
                        char path[256];
                        unsigned int c = (msg[2] != 0) ? read(pipefds[0], path, msg[2]) : 0;
                        if (c != msg[2]) break;
                        path[c] = 0;
                        partition_set_path(msg[1], path);
                    } else if (msg[0] == 2) {
                        partition_select(msg[1]);
                    } else {
                        break;
                    }
                }
            }
            waitpid(pid, &status, 0);
            lastfail = (signed char)WEXITSTATUS(status);
        }
        if (pipefds[0] >= 0) close(pipefds[0]);
    }
#endif
    for (;;) {
        int thisfail = driver->initialize(lastfail);
        int change = thisfail != lastfail;
        if (!thisfail) break;
        lastfail = thisfail;
        if (change) exit(thisfail);
        usleep(1000000);
    }

    errno = 0;
    if (arguments.priority && nice(arguments.priority) == -1 && errno != 0) {
        log_printf("Couldn't adjust priority: %s(%d)", strerror(errno), errno);
    }
    if (!arguments.user || !arguments.group) {
        uid_t uid = getuid(), euid = geteuid();
        if (uid <= 0 || uid != euid) {
            if (!arguments.user) arguments.user = "nobody";
            if (!arguments.group) arguments.group = "nogroup";
        }
    }
    if (arguments.user && !(user2 = getpwnam(arguments.user))) {
        if (lastfail != 3) log_printf("User \"%s\" does not exist", arguments.user);
        exit(3);
    }
    if (arguments.group && !(group2 = getgrnam(arguments.group))) {
        if (lastfail != 4) log_printf("Group \"%s\" does not exist", arguments.group);
        exit(4);
    }
    if (arguments.root) {
        uid_t uid = getuid(), euid = geteuid();
        if (chdir(arguments.root)) {
            if (lastfail != 5) log_printf("Changing dir to \"%s\" failed: %s(%d)", arguments.root, strerror(errno), errno);
            exit(5);
        } else {
            if (chroot(arguments.root) || chdir("/")) {
                if (uid <= 0 || uid != euid) {
                    if (lastfail != 6) log_printf("Chrooting failed: %s(%d)", strerror(errno), errno);
                    exit(6);
                }
                log_printf("Chrooting failed: %s(%d)", strerror(errno), errno);
            }
        }
    }
    if ((group2 && setgid(group2->gr_gid)) || (user2 && setuid(user2->pw_uid))) {
        if (lastfail != 7) log_printf("Could not drop privileges: %s(%d)", strerror(errno), errno);
        exit(7);
    }
#endif
}

#ifdef WIN32
static BOOL CALLBACK MyDlgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
        switch (wParam) {
        case IDOK:
        case IDCANCEL:
        case IDC_EXIT:
            DestroyWindow(hwnd);
            return TRUE;
        case IDC_HIDE:
            ShowWindow(hwnd, SW_HIDE);
            return TRUE;
        }
        break;
    case WM_USER + 1:
        {
            HWND logwindow_handle = GetDlgItem(hwnd, IDC_LOG);
            int len = GetWindowTextLength(logwindow_handle);
            static int max_size;
            if (max_size == 0) max_size = SendMessage(logwindow_handle, EM_GETLIMITTEXT, 0, 0);
            SendMessage(logwindow_handle, WM_SETREDRAW, FALSE, 0);
            while (len + 256 > max_size && max_size > 512) {
                SendMessage(logwindow_handle, EM_SETSEL, 0, SendMessage(logwindow_handle, EM_LINELENGTH, 0, 0) + 2);
                SendMessage(logwindow_handle, EM_REPLACESEL, 0, (LPARAM)"");
                len = GetWindowTextLength(logwindow_handle);
            }
            if (len != 0) {
                SendMessage(logwindow_handle, EM_SETSEL, len, len);
                SendMessage(logwindow_handle, EM_REPLACESEL, 0, (LPARAM)"\r\n");
                len += 2;
            }
            SendMessage(logwindow_handle, EM_REPLACESEL, 0, lParam);
            SendMessage(logwindow_handle, EM_SETSEL, len, len);
            SendMessage(logwindow_handle, WM_SETREDRAW, TRUE, 0);
            SendMessage(logwindow_handle, EM_SCROLLCARET, 0, 0);
            free((void *)lParam);
        }
        break;
    case WM_SIZE:
        {
            RECT r;
            int bottom_margin, top_margin;
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            r.left = 0; r.top = 0; r.right = 170; r.bottom = 105;
            MapDialogRect(hwnd, &r);
            bottom_margin = r.bottom;
            top_margin = r.bottom;
            r.left = 0; r.top = 0; r.right = 170; r.bottom = 80;
            MapDialogRect(hwnd, &r);
            bottom_margin -= r.bottom;
            MoveWindow(GetDlgItem(hwnd, IDC_LOG), r.left, r.top, width, height - bottom_margin, TRUE);

            r.left = 5; r.top = 85; r.right = 5 + 95; r.bottom = 85 + 15;
            MapDialogRect(hwnd, &r);
            top_margin -= r.top;
            MoveWindow(GetDlgItem(hwnd, IDC_HIDE), r.left, height - top_margin, r.right - r.left, r.bottom - r.top, TRUE);
            r.left = 105; r.top = 85; r.right = 105 + 60; r.bottom = 85 + 15;
            MapDialogRect(hwnd, &r);
            MoveWindow(GetDlgItem(hwnd, IDC_EXIT), r.left, height - top_margin, r.right - r.left, r.bottom - r.top, TRUE);
        }
        break;
    case WM_USER:
        if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN) {
            ShowWindow(hwnd, SW_SHOW);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char *argv[]) {
    int b;
#ifdef WIN32
    HWND hwnd;
    MSG msg;
    NOTIFYICONDATA notifyIconData;

    hwnd = CreateDialog(NULL, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)MyDlgProc);
    logwindow_set(hwnd);

    log_print("IDEDOS 0.9x PCLink ideservd " VERSION "\r\n"
    "Written by Kajtar Zsolt <soci@c64.rulez.org>\r\n"
    "Win32 port by J.Soucek <josef@ide64.org>\r\n"
    "http://idedos.ide64.org/\r\n"
    "This program is free software. See the GNU\r\n"
    "General Public License for more details.\r\n");
#else
    unsigned char ec = 0x5a;
#endif
    init(argc, argv);
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);
    for (b = 0; b < 15; b++) buff[b].mode = CM_CLOSED;
    buff[15].mode = CM_ERR;
    if (buffer_reserve(&buff[15], 256)) {
        log_print("Out of memory");
        log_flush();
        return EXIT_FAILURE;
    }
    seterror(ER_DOS_VERSION, 0);

#ifdef WIN32
    if (!arguments.background) ShowWindow(hwnd, SW_SHOW);
    memset(&notifyIconData, 0, sizeof notifyIconData);
    notifyIconData.cbSize = sizeof notifyIconData;
    notifyIconData.hWnd = hwnd;
    notifyIconData.uID = 0;
    notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyIconData.uCallbackMessage = WM_USER;
    notifyIconData.hIcon = (HICON)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
    strcpy(notifyIconData.szTip, "IDE64 PCLink");
    Shell_NotifyIcon(NIM_ADD, &notifyIconData);

    HANDLE hThread;
    DWORD iID;
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PCLinkLoop, 0, 0, &iID);

    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    logwindow_set(NULL);
    TerminateThread(hThread, 0);
    Shell_NotifyIcon(NIM_DELETE, &notifyIconData);
    terminate(0);
    return EXIT_SUCCESS;
}

static long WINAPI PCLinkLoop(long lParam) {
    (void)lParam;
    int b;
    unsigned char ec = 0x5a;
    int lastfail = 0;
    goto start;
#endif //WIN32

    for (;;) {
        log_flush();
    start2:
        crc_clear(0);
        b = driver->wait(ec);
        if (b < 0) {
#ifdef WIN32
            driver->shutdown();
            for (;;) {
                usleep(1000000);
            start:
                lastfail = driver->initialize(lastfail);
                if (!lastfail) break;
            }
            continue;
#else
            terminate(0);
#endif
        }

        switch (b) {
        case 0x00: goto start2;
        case 'N': crc_clear(-1); /* fall through */
        case 0xCE: b = openfile(driver, buff, &arguments, b == 0xCE ? FLAG_USE_CRC : 0); ec = 0x5a; break;
        case 'G': crc_clear(-1); /* fall through */
        case 0xC7: b = readfile(driver, buff, &arguments, b == 0xC7 ? FLAG_USE_CRC : 0); ec = 0x5a; break;
        case 'P': crc_clear(-1); /* fall through */
        case 'S': crc_clear(-1); /* fall through */
        case 0xD3: b = writefile(driver, buff, &arguments, b == 0xD3 ? FLAG_USE_CRC : b == 'P' ? FLAG_USE_PADDING : 0); ec = 0x5a; break;
        case 'D': crc_clear(-1); /* fall through */
        case 0xC4: b = closefile(driver, buff, &arguments, b == 0xC4 ? FLAG_USE_CRC : 0); ec = 0x5a; break;
        case 'I': crc_clear(-1); /* fall through */
        case 0xC9: b = statuserror(driver, (char *)buff[15].data, &arguments, b == 0xC9 ? FLAG_USE_CRC : 0); ec = 0x5a; break;
        case 'O': crc_clear(-1); /* fall through */
        case 0xCF: b = openfile_compat(driver, buff, &arguments, b == 0xCF ? FLAG_USE_CRC : 0); ec = 0; break;
        case 'R': crc_clear(-1); /* fall through */
        case 0xD2: b = readfile_compat(driver, buff, &arguments, b == 0xD2 ? FLAG_USE_CRC : 0); ec = 0; break;
        case 'W': crc_clear(-1); /* fall through */
        case 0xD7: b = writefile_compat(driver, buff, &arguments, b == 0xD7 ? FLAG_USE_CRC : 0); ec = 0; break;
        case 'C': crc_clear(-1); /* fall through */
        case 0xC3: b = closefile_compat(driver, buff, &arguments, b == 0xC3 ? FLAG_USE_CRC : 0); ec = 0; break;
        default: log_printf("Unknown command %02X", b);
        }
        if (b) {
            if (driver->clean()) log_print("Timeout");
        }
    }
    return 0;
}
