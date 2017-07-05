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



#ifndef CURL_FIFO_H_
#define CURL_FIFO_H_

#include <stdint.h>

typedef struct _Curlfifo {
    uint8_t *buffer;
    uint8_t *rptr, *wptr, *end;
    uint32_t rndx, wndx;
} Curlfifo;

Curlfifo *curl_fifo_alloc(unsigned int size);
void curl_fifo_free(Curlfifo *f);
void curl_fifo_reset(Curlfifo *f);
int curl_fifo_size(Curlfifo *f);
int curl_fifo_space(Curlfifo *f);
int curl_fifo_generic_read(Curlfifo *f, void *dest, int buf_size, void (*func)(void*, void*, int));
int curl_fifo_generic_write(Curlfifo *f, void *src, int size, int (*func)(void*, void*, int));
int curl_fifo_realloc2(Curlfifo *f, unsigned int size);
void curl_fifo_drain(Curlfifo *f, int size);

static inline uint8_t curl_fifo_peek(Curlfifo *f, int offs)
{
    uint8_t *ptr = f->rptr + offs;
    if (ptr >= f->end) {
        ptr -= f->end - f->buffer;
    }
    return *ptr;
}

#endif
