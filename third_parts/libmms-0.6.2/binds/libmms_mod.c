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



//by peter,2012,1121
#include<ammodule.h>
#include "libavformat/url.h"
#include <android/log.h>
#define  LOG_TAG    "libmms_mod"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)


ammodule_methods_t  libmms_module_methods;
ammodule_t AMPLAYER_MODULE_INFO_SYM = {
tag:
    AMPLAYER_MODULE_TAG,
version_major:
    AMPLAYER_API_MAIOR,
version_minor:
    AMPLAYER_API_MINOR,
    id: 0,
name: "mms_mod"
    ,
author: "Amlogic"
    ,
descript: "libmms module binding library"
    ,
methods:
    &libmms_module_methods,
dso :
    NULL,
reserved :
    {0},
};

extern URLProtocol ff_mmsx_protocol;
int libmms_mod_init(const struct ammodule_t* module, int flags)
{
    LOGI("libmms module init\n");
    av_register_protocol(&ff_mmsx_protocol);
    return 0;
}


int libmms_mod_release(const struct ammodule_t* module)
{
    LOGI("libmms module release\n");
    return 0;
}


ammodule_methods_t  libmms_module_methods = {
    .init =  libmms_mod_init,
    .release =   libmms_mod_release,
} ;

