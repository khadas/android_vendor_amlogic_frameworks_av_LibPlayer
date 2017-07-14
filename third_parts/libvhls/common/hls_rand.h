/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


//some codes ported from vlc,by peter.

#ifndef HLS_RAND_H
#define HLS_RAND_H


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

/**
 * \file
 * This file defined random number generator function
 */

void vlc_rand_bytes(void *buf, size_t len);
void hls_rand_bytes(void *buf, size_t len);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif
