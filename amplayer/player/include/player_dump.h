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



#ifndef _PLAYER_DUMP_H_
#define _PLAYER_DUMP_H_
#ifdef  __cplusplus
extern "C" {
#endif

int player_dump_playinfo(int pid, int fd);
int player_dump_bufferinfo(int pid, int fd);
int player_dump_tsyncinfo(int pid, int fd);
#ifdef  __cplusplus
}
#endif
#endif

