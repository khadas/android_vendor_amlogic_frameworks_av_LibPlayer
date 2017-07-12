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



#ifndef DASH_COMMON_H_
#define DASH_COMMON_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef ANDROID
#include <utils/Log.h>
#endif
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef ANDROID
#define LOG_TAG "libdash"
#ifndef LOGV
#define LOGV(...)      LOG_PRI(ANDROID_LOG_VERBOSE, LOG_TAG,__VA_ARGS__)
#endif

#ifndef LOGI
#define LOGI(...)      LOG_PRI(ANDROID_LOG_INFO, LOG_TAG,__VA_ARGS__)
#endif

#ifndef LOGW
#define LOGW(...)      LOG_PRI(ANDROID_LOG_WARN, LOG_TAG,__VA_ARGS__)
#endif

#ifndef LOGE
#define LOGE(...)       LOG_PRI(ANDROID_LOG_ERROR, LOG_TAG,__VA_ARGS__)
#endif

#else
#define LOGE(...)  printf
#define LOG_DEFAULT  0

#undef LOGI
#define LOGI(f,s...)  do{char *level1=getenv("DASHLOG_LEVEL"); \
                                       if(level1&&atoi(level1)>LOG_DEFAULT) \
                           fprintf(stderr,f,##s);\
                           else; }while(0);
#define LOGV    LOGI
#endif
#define TRACE()  LOGV("TARCE:%s:%d\n",__FUNCTION__,__LINE__);
#define LITERAL_TO_STRING_INTERNAL(x)    #x
#define LITERAL_TO_STRING(x) LITERAL_TO_STRING_INTERNAL(x)

#define CHECK(condition)                                \
    LOG_ALWAYS_FATAL_IF(                                \
            !(condition),                               \
            "%s",                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)    \
            " CHECK(" #condition ") failed.")


#ifdef __cplusplus
}
#endif

typedef  int (* URL_IRQ_CB)();

#define DASH_SEEK_UNSUPPORT     -55
#define DASH_SESSION_EXIT       -88
#define DASH_SESSION_EOF        -99

// use by flag
#define FIND_NEW_SEGMENT    0x01
#define INIT_SEGMENT        0x02

typedef enum {
    DASH_STYPE_NO = 0,
    DASH_STYPE_AUDIO    = 1 << 0,
    DASH_STYPE_VIDEO = 1 << 1,
    DASH_STYPE_AV = 1 << 2,
} dash_session_type;

typedef enum {
    DASH_SFORMAT_NO = 0,
    DASH_SFORMAT_MP2T   = 1,
    DASH_SFORMAT_MP4 = 2,
} dash_stream_format;

typedef enum {
    DASH_CODECTYPE_NO = 0,
    DASH_CODECTYPE_H264 = 0x01,

    DASH_CODECTYPE_AAC = 0x100,
} dash_codec_type;


#endif
