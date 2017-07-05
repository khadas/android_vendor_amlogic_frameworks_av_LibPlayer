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



#ifndef CURL_LOG_H_
#define CURL_LOG_H_

#undef CLOGI
#undef CLOGE
#undef CLOGV

#ifdef ANDROID
#include <android/log.h>
#ifndef LOG_TAG
#define LOG_TAG "curl-mod"
#endif
#define  CLOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  CLOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define  CLOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define CLOGI(f,s...) fprintf(stderr,f,##s)
#define CLOGE(f,s...) fprintf(stderr,f,##s)
#define CLOGV(f,s...) fprintf(stderr,f,##s)
#endif

#endif