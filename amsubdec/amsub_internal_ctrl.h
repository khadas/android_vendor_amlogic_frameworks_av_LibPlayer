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



#ifndef AMSUB_INTERNAL_H_
#define AMSUB_INTERNAL_H_

#include "amsub_dec.h"


void aml_sub_start(void **amsub_handle, amsub_info_t *amsub_info);

int aml_sub_stop(void *priv);

int aml_sub_release(void **amsub_handle);

int aml_sub_read_odata(void **amsub_handle, amsub_info_t *amsub_info);

#endif
