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



#ifndef STREAM_DECODER_H
#define STREAM_DECODER_H

#include "player_priv.h"


typedef struct stream_decoder {
    char name[16];
    pstream_type type;
    int (*init)(play_para_t *);
    int (*add_header)(play_para_t *);
    int (*release)(play_para_t *);
    int (*stop_async)(play_para_t *);
} stream_decoder_t;

static inline codec_para_t * codec_alloc(void)
{
    return MALLOC(sizeof(codec_para_t));
}
static inline  void codec_free(codec_para_t * codec)
{
    if (codec) {
        FREE(codec);
    }
    codec = NULL;
}
int register_stream_decoder(const stream_decoder_t *decoder);

const stream_decoder_t *find_stream_decoder(pstream_type type);

#endif

