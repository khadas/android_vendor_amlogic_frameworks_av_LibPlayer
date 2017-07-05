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



#ifndef HLS_DEBUG_H_
#define HLS_DEBUG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HLS_LOG_BASE,
    HLS_SHOW_URL,
}
HLS_LOG_LEVEL;

#ifndef LOGV
#define LOGV(...)   fprintf(stderr,__VA_ARGS__)
#endif

#ifndef LOGI
#define LOGI(...)   fprintf(stderr,__VA_ARGS__)
#endif

#ifndef LOGW
#define LOGW(...)   fprintf(stderr,__VA_ARGS__)
#endif

#ifndef LOGE
#define LOGE(...)   fprintf(stderr,__VA_ARGS__)
#endif


#define TRACE()  printf("TARCE:%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);


#define LITERAL_TO_STRING_INTERNAL(x)    #x
#define LITERAL_TO_STRING(x) LITERAL_TO_STRING_INTERNAL(x)

#define CHECK(condition)                                \
    LOGV(                                \
            !(condition),                               \
            "%s",                                       \
            __FILE__ ":" LITERAL_TO_STRING(__LINE__)    \
            " CHECK(" #condition ") failed.")




#ifdef __cplusplus
}
#endif

#endif

