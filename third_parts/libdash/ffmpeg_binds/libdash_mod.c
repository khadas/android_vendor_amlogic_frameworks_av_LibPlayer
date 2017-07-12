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



#include "ammodule.h"
#include "libavformat/url.h"
#include "libavformat/avformat.h"
#ifdef ANDROID
#include <android/log.h>
#define  LOG_TAG    "libdash_mod"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)  printf
#endif
ammodule_methods_t  libdash_module_methods;
ammodule_t AMPLAYER_MODULE_INFO_SYM = {
tag:
    AMPLAYER_MODULE_TAG,
version_major:
    AMPLAYER_API_MAIOR,
version_minor:
    AMPLAYER_API_MINOR,
    id: 0,
name: "dash_mod"
    ,
author: "Amlogic"
    ,
descript: "libdash module binding library"
    ,
methods:
    &libdash_module_methods,
dso :
    NULL,
reserved :
    {0},
};

extern URLProtocol dash_protocol;

extern AVInputFormat ff_dash_demuxer;


int libdash_mod_init(const struct ammodule_t* module, int flags)
{
    LOGI("libdash module init\n");
    av_register_input_format(&ff_dash_demuxer);
    av_register_protocol(&dash_protocol);

    return 0;
}


int libdash_mod_release(const struct ammodule_t* module)
{
    LOGI("libdash module release\n");
    return 0;
}


ammodule_methods_t  libdash_module_methods = {
    .init =  libdash_mod_init,
    .release =   libdash_mod_release,
} ;

