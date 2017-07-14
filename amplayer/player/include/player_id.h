/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
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


