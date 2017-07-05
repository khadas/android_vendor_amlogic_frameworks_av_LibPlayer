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



#include <player_error.h>

#include "stream_decoder.h"
#include "player_priv.h"

#define MAX_DECODER (16)
static const stream_decoder_t *stream_decoder_list[MAX_DECODER];
static int stream_index = 0;

int register_stream_decoder(const stream_decoder_t *decoder)
{
    if (decoder != NULL && stream_index < MAX_DECODER) {
        stream_decoder_list[stream_index++] = decoder;
    } else {
        return PLAYER_FAILED;
    }
    return PLAYER_SUCCESS;
}

const stream_decoder_t *find_stream_decoder(pstream_type type)
{
    int i;
    const stream_decoder_t *decoder;

    for (i = 0; i < stream_index; i++) {
        decoder = stream_decoder_list[i];
        if (type == decoder->type) {
            return decoder;
        }
    }
    return NULL;
}

