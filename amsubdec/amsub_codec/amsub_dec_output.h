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



#ifndef AMSUB_DEC_OUTPUT_H_
#define AMSUB_DEC_OUTPUT_H_

#include "amsub_dec.h"

#define text_sub_num 60
#define bitmap_sub_num 30
int amsub_dec_out_init(amsub_para_t *amsub_para);

int amsub_dec_out_add(amsub_para_t *amsub_para);

int amsub_dec_out_get(amsub_para_t *amsub_para, amsub_info_t *amsub_info);

int amsub_dec_out_close(amsub_para_t *amsub_para);

int get_amsub_size(amsub_para_s *amsub_p);

int amsub_dec_out_ctrl();

void dump_amsub_data_info(amsub_para_s *amsub_para);

void amsub_get_odata_info(amsub_para_s *amsub_para, amsub_info_t *amsub_info);

#endif
