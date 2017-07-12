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



#include <unistd.h>
#include "libavformat/avio.h"
#include "libavutil/avutil.h"
#include "../libdash_wrapper/libdash_wrapper.h"
#include <string.h>

#define FFMPEG_DASH_TAG "dash-protocol"

#define RLOG(...) av_tag_log(FFMPEG_DASH_TAG,__VA_ARGS__)

static int dash_protocol_open(URLContext *h, const char *filename, int flags)
{
    if (h == NULL || filename == NULL || strstr(filename, "dash") == NULL) {
        return AVERROR(EINVAL);
    }

    DASH_CONTEXT* ctx = av_mallocz(sizeof(DASH_CONTEXT));
    if (NULL == ctx) {
        return AVERROR(ENOMEM);
    }

    char listfile[4096] = {0};
    strcpy(listfile, "http");
    strcat(listfile, filename + 4);
    RLOG("[%s:%d]dash_open %s\n", __FUNCTION__, __LINE__, listfile);
    int ret = dash_open(ctx, listfile, url_interrupt_cb);
    if (ret != 0) {
        RLOG("[%s:%d]dash_open failed\n", __FUNCTION__, __LINE__);
        return AVERROR(EINVAL);
    }

    if (ctx->durationUs > 0) {
        h->support_time_seek = 1;
    } else { //live streaming
        h->support_time_seek = 0;
    }
    h->is_slowmedia = 0;
    h->is_streamed = 1;
    h->is_segment_media = 1;
    h->priv_data = ctx;
    RLOG("[%s:%d]dash_open %s successful\n", __FUNCTION__, __LINE__, listfile);
    return 0;
}
static int dash_protocol_read(URLContext *h, unsigned char *buf, int size)
{
    if (h == NULL || h->priv_data == NULL || size <= 0) {
        RLOG("failed to read data\n");
        return AVERROR(EINVAL);
    }

    DASH_CONTEXT* ctx = (DASH_CONTEXT*)h->priv_data;
    if (NULL == ctx || ctx->nb_session <= 0) {
        return AVERROR(EINVAL);
    }

    DASH_SESSION_CONTEXT * lsCtx = &(ctx->sessions[0])  ;

    // AVERROR_EOF
    lsCtx->flags = 1;
    int ret = dash_read(lsCtx, buf, size);
    if (ret == DASH_SESSION_EOF) {
        ret = 0;
        RLOG("[%s:%d] to the AVERROR_EOF ret=%d\n", __FUNCTION__, __LINE__, ret);
    } else if (ret == DASH_SESSION_EXIT) {
        ret = AVERROR_EXIT;
        RLOG("[%s:%d] to the AVERROR_EXIT=%d\n", __FUNCTION__, __LINE__, ret);
    }

    return ret;
}
static int64_t dash_protocol_exseek(URLContext *h, int64_t pos, int whence)
{
    DASH_CONTEXT* ctx = (DASH_CONTEXT*)h->priv_data;
    if (NULL == ctx) {
        return AVERROR(EINVAL);
    }

    if (whence == AVSEEK_FULLTIME) {
        if (ctx->durationUs > 0) {
            return (int64_t)(ctx->durationUs / 1000);       // second
        } else {
            return -1;
        }
    } else if (whence == AVSEEK_TO_TIME) {
        RLOG("Seek to time: pos: %lld\n", pos);
        if (ctx->durationUs > 0 && pos >= 0 && (pos * 1000) < ctx->durationUs) {
            int64_t seekToUs = dash_seek(ctx, pos);
            RLOG("Seek to time:%lld\n", seekToUs);
            if (seekToUs >= 0) {
                return seekToUs;    // second
            }
        }
    }
    return -1;
}
static int dash_protocol_close(URLContext *h)
{
    if (h == NULL) {
        RLOG("Failed call :%s\n", __FUNCTION__);
        return -1;
    }
    RLOG("[%s:%d] doing close \n", __FUNCTION__, __LINE__);
    DASH_CONTEXT* ctx = (DASH_CONTEXT*)h->priv_data;
    if (NULL == ctx) {
        return AVERROR(EINVAL);
    }

    dash_close(ctx);
    av_free(ctx);
    RLOG("[%s:%d] close sucessful \n", __FUNCTION__, __LINE__);

    return 0;
}

URLProtocol dash_protocol = {
    .name              = "dash",
    .url_open               = dash_protocol_open,
    .url_read          = dash_protocol_read,
    .url_seek           = NULL,
    .url_close              = dash_protocol_close,
    .url_exseek             = dash_protocol_exseek, /*same as seek is ok*/
    .url_get_file_handle    = NULL,
    .url_setcmd             = NULL,
    .url_getinfo            = NULL,
};
