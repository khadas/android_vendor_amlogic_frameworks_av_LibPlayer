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



/******************************************************************************

                  版权所有 (C), amlogic

 ******************************************************************************
  文 件 名   : hls_simple_cache.h
  版 本 号   : 初稿
  作    者   : peter
  生成日期   : 2013年3月1日 星期五
  最近修改   :
  功能描述   : hls_simple_cache.c 的头文件
  函数列表   :
  修改历史   :
  1.日    期   : 2013年3月1日 星期五
    作    者   : peter
    修改内容   : 创建文件

******************************************************************************/


#ifndef __HLS_SIMPLE_CACHE_H__
#define __HLS_SIMPLE_CACHE_H__


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */


int hls_simple_cache_alloc(int size_max, void** handle);

int hls_simple_cache_get_free_space(void* handle);
int hls_simple_cache_get_data_size(void* handle);
int hls_simple_cache_reset(void* handle);
int hls_simple_cache_revert(void* handle);
int hls_simple_cache_write(void* handle, void* data, int size);

int hls_simple_cache_read(void* handle, void* buffer, int size);
int hls_simple_cache_free(void* handle);

int hls_simple_cache_grow_space(void* handle, int size);

int hls_simple_cache_move_to_pos(void* handle, int pos);

int hls_simple_cache_get_cache_size(void* handle);

int hls_simple_cache_block_read(void* handle, void* buffer, int size, int wait_us);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* __HLS_SIMPLE_CACHE_H__ */
