/*

 arguments.c - A mostly working PCLink server for IDEDOS 0.9x

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

#include "arguments.h"
#include <stdlib.h>
#include "getopt.h"
#include "message.h"

void testarg(Arguments *arguments, int argc, char *argv[]) {
    int c;

    arguments->nameconversion = NC_FORCEDOT;

    for (;;) {
        static struct option long_options[] = {
#ifdef WIN32
            {"background", no_argument, NULL, 'b'},
#elif defined __DJGPP__
#else
            {"background", no_argument, NULL, 'b'},
            {"user", required_argument, NULL, 'u'},
            {"group", required_argument, NULL, 'g'},
            {"nice", required_argument, NULL, 'n'},
#endif
            {"root", required_argument, NULL, 'r'},
            {"log", required_argument, NULL, 'l'},
            {"allprg", no_argument, NULL, 'P'},
            {"dot-type", no_argument, NULL, 'F'},
            {"comma-type", no_argument, NULL, 'C'},
            {"help", no_argument, NULL, '?'},
            {"usage", no_argument, NULL, 1},
            {"version", no_argument, NULL, 'V'},
            {"hog", no_argument, NULL, 'h'},
            {"debug", no_argument, NULL, 'D'},
            {"verbose", no_argument, NULL, 'v'},
            {"device", required_argument, NULL, 'd'},
            {"mode", required_argument, NULL, 'm'},
            {"lptport", required_argument, NULL, 'p'},
            {"ipaddress", required_argument, NULL, 'i'},
            {"network", required_argument, NULL, 'N'},
            {NULL, no_argument, NULL, 0}
        };
        int option_index = 0;

        c = getopt_long(argc, argv,
#if defined WIN32
                        "m:r:l:CFP?VbhvDd:p:i:N:"
#elif defined __DJGPP__
                        "m:r:l:CFP?VhvDd:p:i:N:"
#else
                        "m:u:g:r:l:n:CFP?VbhvDd:p:i:N:"
#endif
                        , long_options, &option_index);
        if (c == -1) {
            if (optind == argc) break;
            c = '?';
        }

        switch (c) {
        case '?':
            message(
                   "Usage: ideservd [OPTION...]\n"
                   "IDEDOS 0.9x PCLink fileserver\n"
                   "\n"
#if defined WIN32
                   "  -b, --background\t     Fork into background\n"
                   "  -C, --comma-type\t     Create comma file types\n"
                   "  -d, --device=COMx\t     Serial port device (COM1)\n"
                   "  -F, --dot-type\t     Create dot file types\n"
#elif defined __DJGPP__
                   "  -C, --comma-type\t     Create comma file types\n"
                   "  -d, --device=COMx\t     Serial port device (COM1)\n"
                   "  -F, --dot-type\t     Create dot file types\n"
#else
                   "  -b, --background\t     Fork into background\n"
                   "  -C, --comma-type\t     Create comma file types\n"
                   "  -d, --device=DEVICE\t     Device (/dev/parport0 or /dev/ttyS0)\n"
                   "  -F, --dot-type\t     Create dot file types\n"
                   "  -g, --group=GROUP\t     Group under we run (nogroup)\n"
#endif
                   "  -h, --hog\t\t     CPU hog (off)\n"
                   "  -i, --ipaddress=IP\t     IP address of C64 on network\n"
                   "  -l, --log=FILE\t     Logfile (stdout)\n"
                   "  -m, --mode=MODE\t     Mode (x1541, xe1541, xm1541, xa1541,\n"
                   "\t\t\t     pc64, pc64s, rs232, rs232s, usb, vice, eth)\n"
#if defined WIN32 || defined __DJGPP__
#else
                   "  -n, --nice=ADJUST\t     Adjust nice level (-19)\n"
#endif
                   "  -N, --network=NUM\t     Network number (0)\n"
                   "  -p, --lptport=IOPORT\t     Printer port address (0x378)\n"
                   "  -P, --allprg\t\t     Almost everything to PRG\n"
                   "  -r, --root=DIRECTORY\t     Root directory (.)\n"
#if defined WIN32 || defined __DJGPP__
#else
                   "  -u, --user=USER\t     User under we run (nobody)\n"
#endif
                   "  -v, --verbose\t\t     Verbose logging\n"
                   "  -?, --help\t\t     Give this help list\n"
                   "      --usage\t\t     Give a short usage message\n"
                   "  -V, --version\t\t     Print program version\n"
                   "\n"
                   "Mandatory or optional arguments to long options are also mandatory or optional\n"
                   "for any corresponding short options.\n"
                   "\n"
                   "Report bugs to <soci@c64.rulez.org>.\n"
                   );
            exit(EXIT_SUCCESS);
        case 1:
            message(
#ifdef WIN32
                   "Usage: ideservd [-CFhPv?V] [-m MODE] [-d COMx] [-p IOPORT] [-i IP] [-N NUM]\n"
                   "        [-r DIRECTORY] [-l FILE] [--mode MODE] [--allprg] [--comma-type]\n"
                   "        [--dot-type] [--device DEVICE] [--lptport=IOPORT] [--ipaddress IP]\n"
                   "        [--network NUM] [--root=DIRECTORY] [--background] [--log=FILE] [--hog]\n"
                   "        [--verbose] [--help] [--usage] [--version]\n"
#elif defined __DJGPP__
                   "Usage: ideservd [-CFhPv?V] [-m MODE] [-d COMx] [-p IOPORT] [-i IP] [-N NUM]\n"
                   "        [-r DIRECTORY] [-l FILE] [--mode MODE] [--allprg] [--comma-type]\n"
                   "        [--dot-type] [--device DEVICE] [--lptport=IOPORT] [--ipaddress IP]\n"
                   "        [--network NUM] [--root=DIRECTORY] [--log=FILE] [--hog] [--verbose]\n"
                   "        [--help] [--usage] [--version]\n"
#else
                   "Usage: ideservd [-bCFhPv?V] [-m MODE] [-d DEVICE] [-p IOPORT] [-i IP] [-N NUM]\n"
                   "        [-r DIRECTORY] [-l FILE] [-u USER] [-g GROUP] [-n ADJUST] [--mode MODE]\n"
                   "        [--allprg] [--comma-type] [--dot-type] [--device DEVICE]\n"
                   "        [--lptport=IOPORT] [--ipaddress IP] [--network NUM] [--root=DIRECTORY]\n"
                   "        [--group=GROUP] [--user=USER] [--background] [--log=FILE]\n"
                   "        [--nice=ADJUST] [--hog] [--verbose] [--help] [--usage] [--version]\n"
#endif
                  );
            exit(EXIT_SUCCESS);
        case 'V':
            message("ideservd " VERSION "\n");
            exit(EXIT_SUCCESS);
#ifdef WIN32
        case 'b': arguments->background = 1; break;
#elif defined __DJGPP__
#else
        case 'b': arguments->background = 1; break;
        case 'u': arguments->user = optarg; break;
        case 'g': arguments->group = optarg; break;
        case 'n': arguments->priority = strtol(optarg, NULL, 0); break;
#endif
        case 'C': arguments->nameconversion = NC_FORCECOMMA; break;
        case 'P': arguments->nameconversion = NC_IGNOREDOT; break;
        case 'F': arguments->nameconversion = NC_FORCEDOT; break;
        case 'D': arguments->debug = 1; break;
        case 'v': arguments->verbose = 1; break;
        case 'l': arguments->log = optarg; break;
        case 'r': arguments->root = optarg; break;
        case 'h': arguments->hog = 1; break;
        case 'd': arguments->device = optarg; break;
        case 'm': arguments->mode_name = optarg; break;
        case 'p': arguments->lptport = strtol(optarg, NULL, 0) & 0xffff; break;
        case 'i': arguments->sin_addr = optarg; break;
        case 'N': arguments->network = strtol(optarg, NULL, 0) & 0xff; break;
        default:
            exit(EXIT_FAILURE);
        }
    }
}

