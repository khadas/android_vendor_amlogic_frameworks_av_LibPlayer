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



#ifndef CURL_FETCH_H_
#define CURL_FETCH_H_

#include "curl_wrapper.h"

typedef struct _CFContext {
    char uri[MAX_CURL_URI_SIZE];
    char * headers;
    char * relocation;
    curl_prot_type prot_type;
    int thread_quited;
    int perform_retval;
    int http_code;
    int seekable;
    int64_t filesize;
    pthread_t pid;
    pthread_mutex_t quit_mutex;
    pthread_cond_t quit_cond;
    int (*interrupt)(void);
    int (*interruptwithpid)(void *);
    void * parent_thread_id;
    CURLWContext * cwc_h;
    CURLWHandle * cwh_h;
    Curl_Data * cwd;
    struct curl_slist * chunk;
} CFContext;

#ifdef __cplusplus
extern "C" {
#endif

CFContext * curl_fetch_init(const char * uri, const char * headers, int flags);
int curl_fetch_open(CFContext * handle);
int curl_fetch_http_keepalive_open(CFContext * handle, const char * uri);
int curl_fetch_read(CFContext * handle, char * buf, int size);
int64_t curl_fetch_seek(CFContext * handle, int64_t off, int whence);
int curl_fetch_close(CFContext * handle);
int curl_fetch_http_set_headers(CFContext * handle, const char * headers);
int curl_fetch_http_set_cookie(CFContext * handle, const char * cookie);
int curl_fetch_get_info(CFContext * handle, curl_info cmd, uint32_t flag, void * info);
void curl_fetch_register_interrupt(CFContext * handle, interruptcallback pfunc);

////////////// interrupt with pid for nuplayer //////////////
void curl_fetch_register_interrupt_pid(CFContext * handle, interruptcallbackwithpid pfunc);
void curl_fetch_set_parent_pid(CFContext * handle, void * thread_id);

#ifdef __cplusplus
}
#endif

#endif
