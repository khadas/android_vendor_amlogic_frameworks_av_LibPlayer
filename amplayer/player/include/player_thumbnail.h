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



#ifndef PLAYER_THUMBNAIL_H
#define PLAYER_THUMBNAIL_H

#ifdef  __cplusplus
extern "C" {
#endif

void * thumbnail_res_alloc(void);
int thumbnail_find_stream_info(void *handle, const char* filename);
int thumbnail_find_stream_info_end(void *handle);
int thumbnail_decoder_open(void *handle, const char* filename);
int thumbnail_extract_video_frame(void * handle, int64_t time, int flag);
int thumbnail_read_frame(void *handle, char* buffer);
void thumbnail_get_video_size(void *handle, int* width, int* height);
float thumbnail_get_aspect_ratio(void *handle);
void thumbnail_get_duration(void *handle, int64_t *duration);
int thumbnail_get_key_metadata(void* handle, char* key, const char** value);
int thumbnail_get_key_data(void* handle, char* key, const void** data, int* data_size);
void thumbnail_get_video_rotation(void *handle, int* rotation);
int thumbnail_decoder_close(void *handle);
void thumbnail_res_free(void* handle);
int thumbnail_get_tracks_info(void *handle, int *vtracks, int *atracks, int *stracks);

#ifdef  __cplusplus
}
#endif

#endif
