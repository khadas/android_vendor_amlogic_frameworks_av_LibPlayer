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



#ifndef PLAYER_ID_MGT__
#define PLAYER_ID_MGT__

int player_request_pid(void);
int player_release_pid(int pid);
void * player_set_inner_exit_pid(int pid);
void * player_is_inner_exit_pid(int pid);
int player_init_pid_data(int pid, void * data);
void * player_open_pid_data(int pid);
int player_close_pid_data(int pid);
int player_id_pool_init(void);
int player_list_pid(char id[], int size);
int check_pid_valid(int pid);

#endif


