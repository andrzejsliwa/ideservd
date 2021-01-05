/*
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
#include "shorten.h"
#include <string.h>

static size_t delete_spaces(const Petscii *s, Petscii *out) {
    size_t cnt = 0, o = 0, i, il = strlen((const char *)s);
    size_t ac = 0, a;
    int pass, hi = 0;

    for (pass = 0; pass < 2; pass++) {
        if (pass) {
            a = il - 16;
            if (a > cnt) a = cnt;
            ac = cnt / 2;
        }
        for (i = 0; i < il; i++) {
            Petscii ch = s[i];
            if (pass) {
                if (hi) {
                    hi = 0;
                    if (ch >= 'A' && ch <= 'Z') {
                        out[o++] = ch | 0x80;
                        continue;
                    }
                }
                out[o++] = ch;
            }
            if (!((ch & 0x7f) >= 'A' && (ch & 0x7f) <= 'Z') && (((unsigned char)ch) ^ 0x30) >= 10) {
                if (!pass) {
                    cnt++;
                } else {
                    ac += a;
                    if (ac >= cnt) {
                        ac -= cnt;
                        o--; hi = 1;
                    }
                }
            }
        }
    }
    out[o] = 0;
    return o;
}

static size_t delete_vowels(const Petscii *s, Petscii *out) {
    size_t cnt = 0, o = 0, i, il = strlen((const char *)s);
    size_t ac = 0, a;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        if (pass) {
            a = il - 16;
            if (a > cnt) a = cnt;
            ac = cnt / 2;
        }
        for (i = 0; i < il; i++) {
            Petscii ch = s[i];
            if (pass) out[o++] = ch;
            if (i && (ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U')) {
                if (!pass) {
                    cnt++;
                } else {
                    ac += a;
                    if (ac >= cnt) {
                        ac -= cnt;
                        o--;
                    }
                }
            }
        }
    }
    out[o] = 0;
    return o;
}

static size_t delete_vowels2(const Petscii *s, Petscii *out) {
    size_t cnt = 0, o = 0, i, il = strlen((const char *)s);
    size_t ac = 0, a;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        if (pass) {
            a = il - 16;
            if (a > cnt) a = cnt;
            ac = cnt / 2;
        }
        for (i = 0; i < il; i++) {
            Petscii ch = s[i];
            if (pass) out[o++] = ch;
            if (i && (ch == ('A' | 0x80) || ch == ('E' | 0x80) || ch == ('I' | 0x80) || ch == ('O' | 0x80) || ch == ('U' | 0x80))) {
                if (!pass) {
                    cnt++;
                } else {
                    ac += a;
                    if (ac >= cnt) {
                        ac -= cnt;
                        o--;
                    }
                }
            }
        }
    }
    out[o] = 0;
    return o;
}

static size_t delete_duplicate(const Petscii *s, Petscii *out) {
    size_t cnt = 0, o = 0, i, il = strlen((const char *)s);
    size_t ac = 0, a;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        if (pass) {
            a = il - 16;
            if (a > cnt) a = cnt;
            ac = cnt / 2;
        }
        for (i = 0; i < il; i++) {
            Petscii ch = s[i];
            if (pass) out[o++] = ch;
            if (ch == s[i + 1]) {
                if (!pass) {
                    cnt++;
                } else {
                    ac += a;
                    if (ac >= cnt) {
                        ac -= cnt;
                        o--;
                    }
                }
            }
        }
    }
    out[o] = 0;
    return o;
}

static size_t delete_any(const Petscii *s, Petscii *out) {
    size_t o = 0, i, il = strlen((const char *)s);
    size_t a = il - 16, cnt = il - 1;
    size_t ac = cnt / 2;
    if (a > cnt) a = cnt;

    for (i = 0; i < il; i++) {
        out[o++] = s[i];
        if (!i) continue;
        ac += a;
        if (ac >= cnt) {
            ac -= cnt;
            o--;
        }
    }
    out[o] = 0;
    return o;
}

Petscii *shorten(const Petscii *s, size_t l) {
    Petscii *tmp = (Petscii *)malloc((l + 1) * sizeof *tmp);
    if (tmp == NULL) return tmp;
    memcpy(tmp, s, l);
    tmp[l] = 0;

    l = delete_spaces(tmp, tmp);
    if (l <= 16) return tmp;
    l = delete_vowels(tmp, tmp);
    if (l <= 16) return tmp;
    l = delete_vowels2(tmp, tmp);
    if (l <= 16) return tmp;
    l = delete_duplicate(tmp, tmp);
    if (l <= 16) return tmp;
    delete_any(tmp, tmp);
    return tmp;
}
