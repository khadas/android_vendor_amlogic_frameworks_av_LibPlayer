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



#ifndef AMSUB_IO_CTRL_H_
#define AMSUB_IO_CTRL_H_
#include "amsub_dec.h"

int subtitle_poll_sub_fd(int sub_fd, int timeout);
int subtitle_get_sub_size_fd(int sub_fd);
int subtitle_read_sub_data_fd(int sub_fd, char *buf, unsigned int length);
int update_read_pointer(int sub_handle, int flag);

int amsub_read_sub_data(amsub_para_t *amsub_para, amsub_info_t *amsub_info);
int open_sub_device();



#endif
