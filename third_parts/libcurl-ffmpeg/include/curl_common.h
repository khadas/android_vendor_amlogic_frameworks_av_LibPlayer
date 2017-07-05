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



#ifndef CURL_COMMON_H_
#define CURL_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define MAX_CURL_URI_SIZE 4096

/* base time unit is us */
#define SLEEP_TIME_UNIT 10*1000
#define CONNECT_TIMEOUT_THRESHOLD 100*1000*1000
#define SELECT_RETRY_TIMES 30
#define SELECT_RETRY_WHEN_CONNECTING 50

#if EDOM > 0
#define CURLERROR(e) (-(e))   ///< Returns a negative error code from a POSIX error code, to return from library functions.
#else
/* Some platforms have E* and errno already negated. */
#define CURLERROR(e) (e)
#endif

#define CURLMAX(a,b) ((a) > (b) ? (a) : (b))
#define CURLMIN(a,b) ((a) > (b) ? (b) : (a))
#define CURLSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)

void *c_malloc(unsigned int size);
void *c_realloc(void *ptr, unsigned int size);
void c_freep(void **arg);
void c_free(void *arg);
void *c_mallocz(unsigned int size);

int c_strstart(const char *str, const char *pfx, const char **ptr);
int c_stristart(const char *str, const char *pfx, const char **ptr);
char *c_stristr(const char *haystack, const char *needle);
size_t c_strlcpy(char *dst, const char *src, size_t size);
size_t c_strlcat(char *dst, const char *src, size_t size);
size_t c_strlcatf(char *dst, size_t size, const char *fmt, ...);

char * c_strrstr(const char *s, const char *str);

#endif
