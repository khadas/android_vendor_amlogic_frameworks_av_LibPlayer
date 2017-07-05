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



#ifndef HLS_COMMON_H_
#define HLS_COMMON_H_

#include <stdio.h>
#include <stdarg.h>

#include <unistd.h>

#include <utils/Log.h>
#include <ctype.h>
#include <strings.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HLS_LOG_BASE,
    HLS_SHOW_URL,
}
HLS_LOG_LEVEL;

#ifndef LOG_TAG
#define LOG_TAG "vhls"
#endif
#ifndef LOGV
#define LOGV ALOGV
#endif

#ifndef LOGI
#define LOGI ALOGI

#endif

#ifndef LOGW
#define LOGW ALOGW

#endif

#ifndef LOGE
#define LOGE ALOGE

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

#endif
