/*
 * Copyright (C) 2010 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



/******
*  init date: 2013.1.23
*  author: senbai.tao<senbai.tao@amlogic.com>
*  description: this code is part of ffmpeg
******/

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "curl_common.h"

void *c_malloc(unsigned int size)
{
    void *ptr = NULL;

    ptr = malloc(size);
    return ptr;
}

void *c_realloc(void *ptr, unsigned int size)
{
    return realloc(ptr, size);
}

void c_freep(void **arg)
{
    if (*arg) {
        free(*arg);
    }
    *arg = NULL;
}
void c_free(void *arg)
{
    if (arg) {
        free(arg);
    }
}
void *c_mallocz(unsigned int size)
{
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

int c_strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr) {
        *ptr = str;
    }
    return !*pfx;
}

int c_stristart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && toupper((unsigned)*pfx) == toupper((unsigned)*str)) {
        pfx++;
        str++;
    }
    if (!*pfx && ptr) {
        *ptr = str;
    }
    return !*pfx;
}

char *c_stristr(const char *s1, const char *s2)
{
    if (!*s2) {
        return s1;
    }

    do {
        if (c_stristart(s1, s2, NULL)) {
            return s1;
        }
    } while (*s1++);

    return NULL;
}

char * c_strrstr(const char *s, const char *str)
{
    char *p;
    int len = strlen(s);
    for (p = s + len - 1; p >= s; p--) {
        if ((*p == *str) && (memcmp(p, str, strlen(str)) == 0)) {
            return p;
        }
    }
    return NULL;
}

size_t c_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = 0;
    while (++len < size && *src) {
        *dst++ = *src++;
    }
    if (len <= size) {
        *dst = 0;
    }
    return len + strlen(src) - 1;
}

size_t c_strlcat(char *dst, const char *src, size_t size)
{
    size_t len = strlen(dst);
    if (size <= len + 1) {
        return len + strlen(src);
    }
    return len + c_strlcpy(dst + len, src, size - len);
}

size_t c_strlcatf(char *dst, size_t size, const char *fmt, ...)
{
    int len = strlen(dst);
    va_list vl;

    va_start(vl, fmt);
    len += vsnprintf(dst + len, size > len ? size - len : 0, fmt, vl);
    va_end(vl);

    return len;
}