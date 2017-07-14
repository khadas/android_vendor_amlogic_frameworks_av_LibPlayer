/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
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

