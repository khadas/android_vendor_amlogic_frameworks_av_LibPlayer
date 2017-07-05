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



#ifndef PTHREAD_MSG_H
#define PTHREAD_MSG_H
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <player.h>

#include "player_priv.h"


//#define PLAYER_DEBUG
#ifdef PLAYER_DEBUG
void debug_set_player_state(play_para_t *player, player_status status, const char *fn, int line);
#define  set_player_state(player,status) debug_set_player_state(player,status,__func__,__LINE__)
#else
void set_player_state(play_para_t *player, player_status status);

#endif


player_status get_player_state(play_para_t *player);
int    player_thread_wait_exit(play_para_t *player);
int    player_thread_create(play_para_t *player);
int    player_thread_wait(play_para_t *player, int microseconds);
int    wakeup_player_thread(play_para_t *player);


#endif

