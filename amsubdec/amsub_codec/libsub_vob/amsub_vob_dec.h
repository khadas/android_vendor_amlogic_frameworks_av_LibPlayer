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



#ifndef AMSUB_VOB_SUB_H
#define AMSUB_VOB_SUB_H

#include "amsub_dec.h"

int get_vob_spu(char *input_buf, int *buf_size, unsigned length, amsub_para_s *spu);
void vobsub_init_decoder(void);
int vob_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle);

typedef enum {
    FSTA_DSP = 0,
    STA_DSP = 1,
    STP_DSP  = 2,
    SET_COLOR = 3,
    SET_CONTR = 4,
    SET_DAREA = 5,
    SET_DSPXA = 6,
    CHG_COLCON = 7,
    CMD_END = 0xFF,
} CommandID;

#endif
