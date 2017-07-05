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




#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <getopt.h>
#include <player.h>
#include <log_print.h>
//#include <version.h>

int main(int argc, char ** argv)
{
    play_control_t ctrl;
    int pid;

    if (argc < 2) {
        printf("USAG:%s file\n", argv[0]);
        return 0;
    }
    player_init();
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.file_name = argv[1];
    ctrl.video_index = -1;
    ctrl.audio_index = -1;
    ctrl.sub_index = -1;
    ctrl.t_pos = -1;
    pid = player_start(&ctrl, 0);
    if (pid < 0) {
        printf("play failed=%d\n", pid);
        return -1;
    }
    while (!PLAYER_THREAD_IS_STOPPED(player_get_state(pid))) {
        usleep(10000);
    }
    player_stop(pid);
    player_exit(pid);
    printf("play end=%d\n", pid);
    return 0;
}
