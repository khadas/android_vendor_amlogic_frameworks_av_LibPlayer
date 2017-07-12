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


#include "libavutil/avstring.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavformat/avio.h"
#include "libavformat/avformat.h"
#include "libavformat/internal.h"
#include "../libdash_wrapper/libdash_wrapper.h"

extern AVInputFormat ff_mov_demuxer;
//#define PRINT_PTS

typedef struct _STREAM_TIME_CONTEXT {
    int id;
    int64_t next_pts;               // session
    int64_t innerStart_pts;     // start time inner segment
    int64_t segmentStart_pts;       // start time of session
    int64_t duration;

} STREAM_TIME_CONTEXT;

AVRational g_Rational = {1, AV_TIME_BASE};

typedef struct _FFMPEG_DASH_SESSION_CONTEXT {
    AVFormatContext *ctx;           // changed by segment
    AVIOContext * pb;               // don't change dring the session
    unsigned char *buffer;          // Start of the buffer, used by pb
    int buffer_size;                    // Maximum buffer size

    int eof;
    int nb_stream;
    STREAM_TIME_CONTEXT streamInfos[2];
} FFMPEG_DASH_SESSION_CONTEXT;

typedef struct _FFMPEG_DASH_CONTEXT {
    DASH_CONTEXT innerCtx;
    int nb_fsession;
    FFMPEG_DASH_SESSION_CONTEXT fsessions[2];
} FFMPEG_DASH_CONTEXT;

static int          IORead(void *opaque, uint8_t *buf, int buf_size)
{
    URLContext *uc = (URLContext *)opaque;
    if (NULL == uc) {
        return 0;
    }

    DASH_SESSION_CONTEXT* session = (DASH_SESSION_CONTEXT*)uc->priv_data;
    if (NULL == session) {
        return 0;
    }

    int ret = dash_read(session, buf, buf_size);
    if (ret == DASH_SESSION_EXIT) {
        ret = AVERROR_EXIT;
        av_log(NULL, AV_LOG_ERROR, "[%s:%d] to the AVERROR_EXIT\n", __FUNCTION__, __LINE__);
    }
    return ret;
}
/*
static int dash_new_stream(AVFormatContext *s, DASH_STREAM_CONTEXT *dashstream)
{
    bool hasVideo = false;
    bool hasAudio = false;
    if (dashstream->stream_type == DASH_STYPE_AV) {
        hasVideo = true;
        hasAudio = true;
    }
       else if(dashstream->stream_type == DASH_STYPE_VIDEO)
        hasVideo = true;
       else if(dashstream->stream_type == DASH_STYPE_AUDIO)
        hasAudio = true;

    if (hasVideo) {
        AVStream *st = av_new_stream(s, 0);
                if (!st) {
                return AVERROR(ENOMEM);
                }

        st->codec->codec_type = CODEC_TYPE_VIDEO;
                if (dashstream->codec_type & DASH_CODECTYPE_H264)
                    st->codec->codec_id = CODEC_ID_H264;
                st->codec->width = dashstream.width;
                st->codec->height = dashstream.height;
    }

    if (hasAudio) {
        AVStream *st = av_new_stream(s, 0);
                if (!st) {
                return AVERROR(ENOMEM);
                }

        st->codec->codec_type = CODEC_TYPE_AUDIO;
                if (dashstream->codec_type & DASH_CODECTYPE_AAC)
                    st->codec->codec_id = CODEC_ID_AAC;
    }

    return 0;
}
*/
static int dash_parser_next_segment(AVFormatContext *s, int dashIndex, int first)
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)s->priv_data;
    FFMPEG_DASH_SESSION_CONTEXT *lsession = (FFMPEG_DASH_SESSION_CONTEXT *) & (ldashCtx->fsessions[dashIndex]);
    if (NULL == lsession) {
        return AVERROR(EINVAL);
    }

    av_log(s, AV_LOG_INFO, "[%s:%d] dash_parser_next_segment dashIndex=%d, first=%d!\n", __FUNCTION__, __LINE__, dashIndex, first);
    int ret;
    AVFormatContext * lctx = (AVFormatContext *)lsession->ctx;
    lsession->ctx = NULL;
    if (first > 0) {
        // open the iocontext
        lsession->buffer_size = 32 * 1024;
        lsession->buffer = (unsigned char*) av_malloc(lsession->buffer_size);
        if (NULL == lsession->buffer) {
            return AVERROR(ENOMEM);
        }

        URLContext *uc = av_mallocz(sizeof(URLContext));
        if (NULL == uc) {
            return AVERROR(ENOMEM);
        }

        uc->priv_data = (void *) & (ldashCtx->innerCtx.sessions[dashIndex]); // set innersession to aviocontext
        AVIOContext * pb = NULL;
        pb = (void *)avio_alloc_context(lsession->buffer, lsession->buffer_size, 0, uc, IORead, NULL, NULL);
        if (NULL == pb) {
            av_log(s, AV_LOG_ERROR, "[%s:%d] avio_alloc_context faileddashIndex = %d!\n", __FUNCTION__, __LINE__, dashIndex);
            return AVERROR(ENOMEM);
        }
        pb->support_time_seek = 0;
        pb->is_slowmedia = 0;   // unuse loop buffer
        pb->is_streamed = 1;
        pb->seekable = 0;
        av_log(s, AV_LOG_INFO, "[%s:%d] open the iocontext dashIndex = %d!\n", __FUNCTION__, __LINE__, dashIndex);
        lsession->pb = pb;
    } else {
        if (lctx != NULL) {
            // no close the aviocontext, no use av_close_input_file
            //lctx->pb = NULL;
            av_close_input_stream(lctx);
        }

        if (lsession->pb != NULL) {
            avio_reset(lsession->pb, 0);
            lsession->pb->support_time_seek = 0;
            lsession->pb->is_slowmedia = 0; // unuse loop buffer
            lsession->pb->is_streamed = 1;
            lsession->pb->seekable = 0;
        }
    }
    lctx = NULL;

    if (!(lctx = avformat_alloc_context())) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] avformat_alloc_context failed!\n", __FUNCTION__, __LINE__);
        return AVERROR(ENOMEM);
    }

    ldashCtx->innerCtx.sessions[dashIndex].flags = 1;   // read an new segment
    lctx->pb = lsession->pb;
    ret = avformat_open_input(&lctx, "", NULL, NULL);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] avformat_open_input failed ret = %d!\n", __FUNCTION__, __LINE__, ret);
        if (lctx != NULL) {
            //lctx->pb = NULL;
            av_close_input_stream(lctx);
        }
        return ret;
    }

    lctx->is_dash_demuxer = 1;
    ret = av_find_stream_info(lctx);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] av_find_stream_info failed!\n", __FUNCTION__, __LINE__);
        if (lctx != NULL) {
            //lctx->pb = NULL;
            av_close_input_stream(lctx);
        }
        return ret;
    }

    int j;
    if (first > 0) {
        for (j = 0; j < (int)lctx->nb_streams ; j++)   {
            AVStream *st = av_new_stream(s, lsession->nb_stream);
            if (NULL == st) {
                av_log(s, AV_LOG_ERROR, "[%s:%d] av_new_stream failed!\n", __FUNCTION__, __LINE__);
                if (lctx != NULL) {
                    //lctx->pb = NULL;
                    av_close_input_stream(lctx);
                }
                return AVERROR(ENOMEM);
            }

            st->time_base.num = g_Rational.num;//lctx->streams[j]->time_base.num;
            st->time_base.den = g_Rational.den;//lctx->streams[j]->time_base.den;
            st->r_frame_rate.num = lctx->streams[j]->r_frame_rate.num;
            st->r_frame_rate.den = lctx->streams[j]->r_frame_rate.den;
            avcodec_copy_context(st->codec, lctx->streams[j]->codec);

            lsession->nb_stream++;
            lsession->streamInfos[j].id = st->index;
            lsession->streamInfos[j].segmentStart_pts = 0;
            lsession->streamInfos[j].innerStart_pts = -1;
            //lsession->streamInfos[j].duration = (st->r_frame_rate.den * st->time_base.num) /(st->r_frame_rate.num * st->time_base.den);
            av_log(s, AV_LOG_INFO, "[%s:%d] newstream dashIndex = %d, streamindex=%d, num=%d, den=%d\n", __FUNCTION__, __LINE__, dashIndex, st->index, st->time_base.num, st->time_base.den);
        }

        s->duration = ldashCtx->innerCtx.durationUs * 1000;
    } else {
        for (j = 0; j < (int)lsession->nb_stream ; j++) {
            lsession->streamInfos[j].innerStart_pts = -1;
            lsession->streamInfos[j].segmentStart_pts = lsession->streamInfos[j].next_pts;
        }
    }

    av_log(s, AV_LOG_INFO, "[%s:%d] read a new segment dashIndex=%d, nb_streams=%d!\n",
           __FUNCTION__, __LINE__, dashIndex, lsession->nb_stream);
    lsession->ctx = lctx;
    return 0;
}

static int choose_dash_session(AVFormatContext *s)
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)s->priv_data;
    if (NULL == ldashCtx) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d] ldashCtx is null!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (ldashCtx->nb_fsession <= 0) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d] ldashCtx nb_fsession <= 0!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    FFMPEG_DASH_SESSION_CONTEXT* lsession = NULL;
    int64_t minpts  = -1;
    int chooseindex = -1;
    int index = 0;
    int64_t localpts = -1;
    for (; index < ldashCtx->nb_fsession; index++) {
        if (ldashCtx->fsessions[index].eof == 1) {
            continue;
        }

        localpts = ldashCtx->fsessions[index].streamInfos[0].next_pts; //av_rescale_q(ldashCtx->fsessions[index].streamInfos[0].next_pts, ldashCtx->fsessions[index].ctx->streams[0]->time_base, g_Rational);
        if (chooseindex == -1 || minpts > localpts) {
            minpts = localpts;
            chooseindex = index;
        }
    }
    return chooseindex;
}

static int dash_read_probe(AVProbeData *p)
{
    av_log(NULL, AV_LOG_INFO, "[%s:%d] dash_probe!\n", __FUNCTION__, __LINE__);
    if ((av_strstart(p->filename, "http:", NULL) != 0 || av_strstart(p->filename, "shttp:", NULL) != 0 || av_strstart(p->filename, "https:", NULL) != 0) && av_stristr(p->filename, ".mpd") != NULL) {
        char *url = p->filename;

        if (av_strstart(p->filename, "shttp:", NULL) != 0) {
            url = p->filename + 1;
        }

        if (dash_probe(url, url_interrupt_cb) != DASH_SFORMAT_MP4) {
            return 0;
        }

        av_log(NULL, AV_LOG_INFO, "[%s:%d] dash_probe choose the dash demuxer!\n", __FUNCTION__, __LINE__);
        return AVPROBE_SCORE_MAX;

    }
    return 0;
}
#ifdef PRINT_PTS
int g_videoprintCnt = 21;
int g_audioprintCnt = 31;
#endif
static int dash_read_header(AVFormatContext *s, AVFormatParameters *ap)
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)(s->priv_data);
    if (NULL == ldashCtx) {
        return AVERROR(EINVAL);
    }

    int ret = 0;
    char *url = s->filename;
    if (av_strstart(s->filename, "shttp:", NULL) != 0) {
        url = s->filename + 1;
    }
    ret = dash_open(&(ldashCtx->innerCtx), url, url_interrupt_cb);
    if (ret != 0) {
        return AVERROR(EINVAL);
    }
    ldashCtx->nb_fsession = ldashCtx->innerCtx.nb_session;

    int index = 0;
    for (; index < ldashCtx->nb_fsession; index++) {
        ret = dash_parser_next_segment(s, index, 1);
        if (ret != 0) {
            av_log(s, AV_LOG_ERROR, "[%s:%d] read header failed! need to close dash context!\n", __FUNCTION__, __LINE__);
            dash_close(&(ldashCtx->innerCtx));
            return ret;
        }
    }
    av_log(s, AV_LOG_INFO, "[%s:%d] successful dash sesison=%d, stream=%d!\n", __FUNCTION__, __LINE__,  ldashCtx->nb_fsession, s->nb_streams);
    return 0;
}
static int dash_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)(s->priv_data);
    if (NULL == ldashCtx) {
        return AVERROR(EINVAL);
    }

    int ret = 0;
    AVFormatContext * lsCtx = NULL;
    int oldindex = 0;
    int curIndex = 0;

retry_choosesession:
    curIndex = choose_dash_session(s);
    if (curIndex < 0) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] demuxer to the end!\n", __FUNCTION__, __LINE__);
        return AVERROR_EOF;
    }

retry_read:
    lsCtx = ldashCtx->fsessions[curIndex].ctx;
    if (NULL == lsCtx) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] lsCtx is null!\n", __FUNCTION__, __LINE__);
        return AVERROR(EINVAL);
    }

    ret = av_read_frame(lsCtx, pkt);
    if (ret == AVERROR_EOF) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] File to EOF!\n", __FUNCTION__, __LINE__);
    } else if (ret == AVERROR(EAGAIN)) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] retry_read!\n", __FUNCTION__, __LINE__);
        goto retry_read;
    } else if (ret == AVERROR_EXIT) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] AVERROR_EXIT!\n", __FUNCTION__, __LINE__);
        return ret;
    } else if (ret == DASH_SESSION_EOF) {
        ldashCtx->fsessions[curIndex].eof = 1;
        av_log(s, AV_LOG_ERROR, "[%s:%d] session = %d to the end!\n", __FUNCTION__, __LINE__, curIndex);
        goto retry_choosesession;
    }

    if (ret < 0) {
        ret = dash_parser_next_segment(s, curIndex, 0);
        if (ret >= 0) {
            av_log(s, AV_LOG_ERROR, "[%s:%d] dash_parser_next_segment retry_read!\n", __FUNCTION__, __LINE__);
            goto retry_read;
        } else if (ret == AVERROR_EXIT) {
            av_log(s, AV_LOG_ERROR, "[%s:%d] dash_parser_next_segment AVERROR_EXIT!\n", __FUNCTION__, __LINE__);
            return ret;
        } else if (ret == DASH_SESSION_EOF) {
            ldashCtx->fsessions[curIndex].eof = 1;
            av_log(s, AV_LOG_ERROR, "[%s:%d] session = %d to the end!\n", __FUNCTION__, __LINE__, curIndex);
            goto retry_choosesession;
        } else {
            av_log(s, AV_LOG_ERROR, "[%s:%d] dash_parser_next_segment failed!\n", __FUNCTION__, __LINE__);
            return ret;
        }
    }

    //av_log(s, AV_LOG_ERROR, "[%s:%d]  pkt->pts=%llx!\n", __FUNCTION__, __LINE__, pkt->pts);
    oldindex = pkt->stream_index;
    if (pkt->pts != AV_NOPTS_VALUE) {
        pkt->pts = av_rescale_q(pkt->pts, lsCtx->streams[oldindex]->time_base, g_Rational);
    }

    if (pkt->dts != AV_NOPTS_VALUE) {
        pkt->dts = av_rescale_q(pkt->dts, lsCtx->streams[oldindex]->time_base, g_Rational);
    }

    pkt->stream_index = ldashCtx->fsessions[curIndex].streamInfos[oldindex].id;
    if (pkt->pts != AV_NOPTS_VALUE) {
        // inner offset
        if (ldashCtx->fsessions[curIndex].streamInfos[oldindex].innerStart_pts == -1) {
            ldashCtx->fsessions[curIndex].streamInfos[oldindex].innerStart_pts = pkt->pts;
        }
        pkt->pts = pkt->pts - ldashCtx->fsessions[curIndex].streamInfos[oldindex].innerStart_pts;
        pkt->dts = pkt->dts - ldashCtx->fsessions[curIndex].streamInfos[oldindex].innerStart_pts;

        // session offset
        //if(memcmp(lsCtx->iformat->name,"mpegts",6) != 0){ // not mpegts
        pkt->pts = pkt->pts + ldashCtx->fsessions[curIndex].streamInfos[oldindex].segmentStart_pts;
        pkt->dts = pkt->dts + ldashCtx->fsessions[curIndex].streamInfos[oldindex].segmentStart_pts;
        //}

        if (pkt->duration > 0) {
            ldashCtx->fsessions[curIndex].streamInfos[oldindex].next_pts = pkt->pts + pkt->duration;
        } else {
            ldashCtx->fsessions[curIndex].streamInfos[oldindex].next_pts = pkt->pts + ldashCtx->fsessions[curIndex].streamInfos[oldindex].duration;
        }
    }

#ifdef PRINT_PTS
    if (lsCtx->streams[oldindex]->codec->codec_type == CODEC_TYPE_VIDEO) {
        g_videoprintCnt++;
        if (g_videoprintCnt > 20) {
            av_log(s, AV_LOG_ERROR, "Video pts=%lld, dts=%lld\n", pkt->pts, pkt->dts);
            g_videoprintCnt = 0;
        }
    } else {
        g_audioprintCnt++;
        if (g_audioprintCnt > 30) {
            av_log(s, AV_LOG_ERROR, "Audio pts=%lld, dts=%lld\n", pkt->pts, pkt->dts);
            g_audioprintCnt = 0;
        }
    }
#endif

    return 0;
}
static int dash_read_seek(AVFormatContext *s, int stream_index,   int64_t timestamp, int flags)     // stream_index:AV_TIME_BASE
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)s->priv_data;
    if (NULL == ldashCtx) {
        return -1;
    }

    int index = 0;
    int pos = 0;
    if (stream_index == -1) {
        pos = av_rescale_rnd(timestamp, 1, AV_TIME_BASE, AV_ROUND_ZERO);    //pts->s
    } else {
        AVStream *st = s->streams[stream_index];
        pos = av_rescale_rnd(timestamp, st->time_base.num, st->time_base.den, AV_ROUND_ZERO); //pts->s
    }
    int reallyPos =  dash_seek(&(ldashCtx->innerCtx), pos);
    if (reallyPos == DASH_SEEK_UNSUPPORT) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] dash_seek DASH_SEEK_UNSUPPORT!\n", __FUNCTION__, __LINE__);
        return 0;
    }
    if (reallyPos < 0) {
        av_log(s, AV_LOG_ERROR, "[%s:%d] dash_seek failed reallyPos=%d, pos=%d!\n", __FUNCTION__, __LINE__, reallyPos, pos);
        return -1;
    }

    // reset the iocontext first
    for (; index < ldashCtx->nb_fsession; index++) {
        avio_reset(ldashCtx->fsessions[index].pb, AVIO_FLAG_READ);
        ldashCtx->fsessions[index].eof = 0;
    }

    //int64_t start_pts= av_rescale_rnd(reallyPos,1,AV_TIME_BASE,AV_ROUND_ZERO);//pts->s;
    index = 0;
    int j = 0;
    for (; index < ldashCtx->nb_fsession; index++) {
        if (dash_parser_next_segment(s, index, 0) != 0) {
            av_log(s, AV_LOG_ERROR, "[%s:%d] dash_parser_next_segment index=%d, reallyPos=%d, pos=%d!\n", __FUNCTION__, __LINE__, index, reallyPos, pos);
            return -1;
        }
        for (j = 0; j < (int)ldashCtx->fsessions[index].nb_stream; j++) {
            ldashCtx->fsessions[index].streamInfos[j].segmentStart_pts = ldashCtx->innerCtx.sessions[index].seekStartTime;//av_rescale_rnd(ldashCtx->innerCtx.sessions[index].seekStartTime,
            //ldashCtx->fsessions[index].streamInfos[j].time_base.den, 1000* ldashCtx->fsessions[index].streamInfos[j].time_base.num,AV_ROUND_ZERO);
            ldashCtx->fsessions[index].streamInfos[j].next_pts = 0;
            ldashCtx->fsessions[index].streamInfos[j].innerStart_pts = -1;
            av_log(s, AV_LOG_INFO, "[%s:%d] index=%d, streamid=%d,Start_pts=%lld!\n", __FUNCTION__, __LINE__, index, j, ldashCtx->fsessions[index].streamInfos[j].segmentStart_pts);
        }
    }

    av_log(s, AV_LOG_INFO, "[%s:%d] reallyPos=%d, pos=%d!\n", __FUNCTION__, __LINE__, reallyPos, pos);
    return reallyPos;
}
static int dash_read_close(AVFormatContext *s)
{
    FFMPEG_DASH_CONTEXT* ldashCtx = (FFMPEG_DASH_CONTEXT *)s->priv_data;
    if (NULL == ldashCtx) {
        return -1;
    }
    av_log(s, AV_LOG_INFO, "[%s:%d]dash sesison=%d, stream=%d!\n", __FUNCTION__, __LINE__,  ldashCtx->nb_fsession, s->nb_streams);
    AVFormatContext * sCtx = NULL;
    AVIOContext * pb = NULL;
    URLContext *h = NULL;
    int index = 0;
    for (; index < ldashCtx->nb_fsession; index++) {
        av_log(s, AV_LOG_INFO, "[%s:%d] close  index=%d!\n", __FUNCTION__, __LINE__,  index);
        if (ldashCtx->fsessions[index].ctx != NULL) {
            sCtx = (AVFormatContext *)ldashCtx->fsessions[index].ctx;
            //sCtx->pb = NULL;
            av_close_input_stream(sCtx);
            ldashCtx->fsessions[index].ctx = NULL;
        }

        if (ldashCtx->fsessions[index].pb != NULL) {
            pb = ldashCtx->fsessions[index].pb;
            h = pb->opaque;
            if (pb && pb->buffer) {
                av_free(pb->buffer);
                av_free(pb);
                pb = NULL;
            }
            if (h != NULL) {
                av_free(h);
                h = NULL;
            }
            //avio_close(pb);
            ldashCtx->fsessions[index].pb = NULL;
        }
    }

    dash_close(&(ldashCtx->innerCtx));
    av_log(s, AV_LOG_INFO, "[%s:%d] successful!\n", __FUNCTION__, __LINE__);
    return 0;
}

AVInputFormat ff_dash_demuxer = {
    "dash",
    NULL,
    sizeof(FFMPEG_DASH_CONTEXT),
    dash_read_probe,
    dash_read_header,
    dash_read_packet,
    dash_read_close,
    dash_read_seek,
    .flags = AVFMT_NOFILE
};

