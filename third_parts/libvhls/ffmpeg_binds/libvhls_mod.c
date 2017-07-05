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
#include "ammodule.h"
#include "libavformat/url.h"
#include "libavformat/avformat.h"
#include <android/log.h>
#define  LOG_TAG    "libvhls_mod"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)


ammodule_methods_t  libvhls_module_methods;
ammodule_t AMPLAYER_MODULE_INFO_SYM = {
tag:
    AMPLAYER_MODULE_TAG,
version_major:
    AMPLAYER_API_MAIOR,
version_minor:
    AMPLAYER_API_MINOR,
    id: 0,
name: "vhls_mod"
    ,
author: "Amlogic"
    ,
descript: "libvhls module binding library"
    ,
methods:
    &libvhls_module_methods,
dso :
    NULL,
reserved :
    {0},
};

extern URLProtocol vhls_protocol;
extern URLProtocol vrwc_protocol;
extern AVInputFormat ff_mhls_demuxer; // hls demuxer for media group

int libvhls_mod_init(const struct ammodule_t* module, int flags)
{
    LOGI("libvhls module init\n");
    av_register_protocol(&vhls_protocol);
    av_register_protocol(&vrwc_protocol);//add for verimatrix drm link
    av_register_input_format(&ff_mhls_demuxer);
    return 0;
}


int libvhls_mod_release(const struct ammodule_t* module)
{
    LOGI("libvhls module release\n");
    return 0;
}


ammodule_methods_t  libvhls_module_methods = {
    .init =  libvhls_mod_init,
    .release =   libvhls_mod_release,
} ;

