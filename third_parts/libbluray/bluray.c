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


#include <libbluray/bluray.h>
#include <util/log_control.h>
#include <libbluray/decoders/overlay.h>
#include <libbluray/bdnav/clpi_data.h>
#include <libbluray/bdnav/navigation.h>
#include <libbluray/bdnav/mpls_parse.h>
#include <cutils/properties.h>
#include "libavutil/avstring.h"
#include "libavformat/avformat.h"
#include "libavformat/url.h"
#include "libavutil/opt.h"
#include "player_type.h"

#include "Amsyswrite.h"
#include "amthreadpool.h"

#define BLURAY_PROTO_PREFIX     "bluray:"
#define MIN_PLAYLIST_LENGTH     180     /* 3 min */

typedef struct {
    const AVClass *class;

    BLURAY *bd;

    int playlist;
    int angle;
    int chapter;
    int64_t duration;
    /*int region;*/
    bluray_info_t bluray_info;
} BlurayContext;

#define OFFSET(x) offsetof(BlurayContext, x)
static const AVOption options[] = {
{"playlist", "", OFFSET(playlist), FF_OPT_TYPE_INT, { .dbl=-1 }, -1,  99999, AV_OPT_FLAG_DECODING_PARAM },
{"angle",    "", OFFSET(angle),    FF_OPT_TYPE_INT, { .dbl=0 },   0,   0xfe, AV_OPT_FLAG_DECODING_PARAM },
{"chapter",  "", OFFSET(chapter),  FF_OPT_TYPE_INT, { .dbl=0 },   0, 0xfffe, AV_OPT_FLAG_DECODING_PARAM },
{"duration",  "", OFFSET(duration),  FF_OPT_TYPE_INT64, { .dbl=0 },   0, INT64_MAX, AV_OPT_FLAG_DECODING_PARAM },
/*{"region",   "bluray player region code (1 = region A, 2 = region B, 4 = region C)", OFFSET(region), AV_OPT_TYPE_INT, { .i64=0 }, 0, 3, AV_OPT_FLAG_DECODING_PARAM },*/
{NULL}
};

static const AVClass bluray_context_class = {
    .class_name     = "bluray",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};


static int check_disc_info(URLContext *h)
{
    BlurayContext *bd = h->priv_data;
    const BLURAY_DISC_INFO *disc_info;

    disc_info = bd_get_disc_info(bd->bd);
    if (!disc_info) {
        av_log(h, AV_LOG_ERROR, "get_disc_info failed\n");
        return -1;
    }

    if (!disc_info->bluray_detected) {
        av_log(h, AV_LOG_ERROR, "BluRay disc not detected\n");
        return -1;
    }

    /* AACS */
    if (disc_info->aacs_detected && !disc_info->aacs_handled) {
        if (!disc_info->libaacs_detected) {
            av_log(h, AV_LOG_ERROR,
                   "Media stream encrypted with AACS, install and configure libaacs\n");
        } else {
            av_log(h, AV_LOG_ERROR, "Your libaacs can't decrypt this media\n");
        }
        return -1;
    }

    /* BD+ */
    if (disc_info->bdplus_detected && !disc_info->bdplus_handled) {
        /*
        if (!disc_info->libbdplus_detected) {
            av_log(h, AV_LOG_ERROR,
                   "Media stream encrypted with BD+, install and configure libbdplus");
        } else {
        */
            av_log(h, AV_LOG_ERROR, "Unable to decrypt BD+ encrypted media\n");
        /*}*/
        return -1;
    }

    return 0;
}

static int bluray_close(URLContext *h)
{
    BlurayContext *bd = h->priv_data;
    if (bd->bd) {
        bd_close(bd->bd);
    }
    if (bd->bluray_info.stream_path) {
        av_free(bd->bluray_info.stream_path);
        bd->bluray_info.stream_path = NULL;
    }
    if (bd->bluray_info.stream_info_num > 0 && bd->bluray_info.stream_info) {
        av_free(bd->bluray_info.stream_info);
        bd->bluray_info.stream_info = NULL;
    }
    if (bd->bluray_info.chapter_num > 0 && bd->bluray_info.chapter_info) {
        av_free(bd->bluray_info.chapter_info);
        bd->bluray_info.chapter_info = NULL;
    }

    return 0;
}

static void overlay_proc(void *context, const BD_OVERLAY * const ov)
{
    BlurayContext *bd = (BlurayContext *)context;

    if (!bd)
        return;
    if (!ov)
        return;
#if LOG_ENABLE
    av_log(NULL, AV_LOG_INFO, "[%s]cmd(%d)\n", __FUNCTION__, ov->cmd);
#endif
    switch (ov->cmd) {
        case BD_OVERLAY_DRAW:
#if LOG_ENABLE
            av_log(NULL, AV_LOG_INFO, "[%s]BD_OVERLAY_DRAW x(%d) y(%d) w(%d) h(%d)\n", __FUNCTION__, ov->x, ov->y, ov->w, ov->h);
#endif
            break;
        default:
            break;
    }
}

static void dump_titles(URLContext *h, BLURAY *bd);

static int bluray_open(URLContext *h, const char *path, int flags)
{
    BlurayContext *bd = h->priv_data;
    int num_title_idx;
    char value[256];
    const char *diskname = path + strlen(BLURAY_PROTO_PREFIX);
    // av_strstart(path, BLURAY_PROTO_PREFIX, &diskname);

    bd->bluray_info.info = -1;
    bd->bluray_info.stream_path = NULL;
    bd->bluray_info.stream_info_num = 0;
    bd->bluray_info.stream_info = NULL;

    bd_set_debug_mask(0);
    // bd_set_debug_mask(0x3FFFFF & (~DBG_STREAM));
    bd->bd = bd_open(diskname, NULL);
    if (!bd->bd) {
        av_log(h, AV_LOG_ERROR, "open failed\n");
        return AVERROR(EIO);
    }

    /* check if disc can be played */
    if (check_disc_info(h) < 0) {
        return AVERROR(EIO);
    }

    // bd_register_overlay_proc(bd->bd, bd, overlay_proc);

    /* setup player registers */
    /* region code has no effect without menus
    if (bd->region > 0 && bd->region < 5) {
        av_log(h, AV_LOG_INFO, "setting region code to %d (%c)\n", bd->region, 'A' + (bd->region - 1));
        bd_set_player_setting(bd->bd, BLURAY_PLAYER_SETTING_REGION_CODE, bd->region);
    }
    */

    /* load title list */
    num_title_idx = bd_get_titles(bd->bd, TITLES_RELEVANT, MIN_PLAYLIST_LENGTH);
#if LOG_ENABLE
    av_log(h, AV_LOG_INFO, "%d usable playlists:\n", num_title_idx);
#endif
    if (num_title_idx < 1) {
        return AVERROR(EIO);
    }

#if LOG_ENABLE
    av_log(h, AV_LOG_INFO, "bd playlist: %d\n", bd->playlist);
#endif
    /* if playlist was not given, select longest playlist */
    if (bd->playlist < 0) {
        uint64_t duration = 0;
        int i;
        for (i = 0; i < num_title_idx; i++) {
            BLURAY_TITLE_INFO *info = bd_get_title_info(bd->bd, i, 0);

#if LOG_ENABLE
            av_log(h, AV_LOG_INFO, "playlist %05d.mpls (%d:%02d:%02d)\n",
                   info->playlist,
                   ((int)(info->duration / 90000) / 3600),
                   ((int)(info->duration / 90000) % 3600) / 60,
                   ((int)(info->duration / 90000) % 60));
#endif

            if (info->duration > duration) {
                bd->playlist = info->playlist;
                duration = info->duration;
                bd->duration = duration;
            }

            bd_free_title_info(info);
        }
#if LOG_ENABLE
        av_log(h, AV_LOG_INFO, "selected %05d.mpls\n", bd->playlist);
#endif
    }

    /* select playlist */
    if (bd_select_playlist(bd->bd, bd->playlist) <= 0) {
#if LOG_ENABLE
        av_log(h, AV_LOG_ERROR, "select playlist(%05d.mpls) failed\n", bd->playlist);
#endif
        return AVERROR(EIO);
    }
    property_get("media.libplayer.dumpbd", value, NULL);
    if (atoi(value))
        dump_titles(h, bd->bd);

    char *stream_path = bd_stream_path(bd->bd);
    if (stream_path) {
#if LOG_ENABLE
        av_log(h, AV_LOG_INFO, "stream_path(%s)\n", stream_path);
#endif
        bd->bluray_info.info = BLURAY_STREAM_PATH;
        bd->bluray_info.stream_path = stream_path;

#if LOG_ENABLE
        av_log(h, AV_LOG_INFO, "current title(%d)\n", bd_get_current_title(bd->bd));
#endif
        CLPI_CL *clpi = bd_get_clpi(bd->bd);
        av_log(h, AV_LOG_INFO, "clpi(%x)\n", clpi);
        if (!clpi)
            return AVERROR(EIO);
        int numProg = clpi->program.num_prog;
#if LOG_ENABLE
        av_log(h, AV_LOG_INFO, "numProg(%d)\n", numProg);
#endif
        int i;
        int stream_count = 0;
        for (i = 0; i < numProg; i++) {
            CLPI_PROG prog = clpi->program.progs[i];
            int numStreams = prog.num_streams;
#if LOG_ENABLE
            av_log(h, AV_LOG_INFO, "numStreams(%d)\n", numStreams);
#endif
            stream_count += numStreams;
        }
        if (stream_count > 0) {
            bd->bluray_info.stream_info_num = stream_count;
            int size = sizeof(bluray_stream_info_t) * stream_count;
            bd->bluray_info.stream_info = (bluray_stream_info_t *)av_malloc(size);
            memset(bd->bluray_info.stream_info, 0, size);

            int stream_index = 0;
            for (i = 0; i < numProg; i++) {
                CLPI_PROG prog = clpi->program.progs[i];
                int numStreams = prog.num_streams;
                int j;
                for (j = 0; j < numStreams; j++) {
                    CLPI_PROG_STREAM stream = prog.streams[j];
#if LOG_ENABLE
                    av_log(h, AV_LOG_INFO, "pid(%x) coding_type(%x) format(%x) rate(%d) aspect(%d) oc_flag(%d) char_code(%d) lang(%s)\n",
                            stream.pid, stream.coding_type, stream.format, stream.rate, stream.aspect, stream.oc_flag, stream.char_code, stream.lang);
#endif
                    switch (stream.coding_type) {
                        case BLURAY_STREAM_TYPE_VIDEO_MPEG1:
                        case BLURAY_STREAM_TYPE_VIDEO_MPEG2:
                        case BLURAY_STREAM_TYPE_VIDEO_VC1:
                        case BLURAY_STREAM_TYPE_VIDEO_H264:
                            bd->bluray_info.stream_info[stream_index].type = BLURAY_STREAM_TYPE_VIDEO;
                            break;
                        case BLURAY_STREAM_TYPE_AUDIO_MPEG1:
                        case BLURAY_STREAM_TYPE_AUDIO_MPEG2:
                        case BLURAY_STREAM_TYPE_AUDIO_LPCM:
                        case BLURAY_STREAM_TYPE_AUDIO_AC3:
                        case BLURAY_STREAM_TYPE_AUDIO_DTS:
                        case BLURAY_STREAM_TYPE_AUDIO_TRUHD:
                        case BLURAY_STREAM_TYPE_AUDIO_AC3PLUS:
                        case BLURAY_STREAM_TYPE_AUDIO_DTSHD:
                        case BLURAY_STREAM_TYPE_AUDIO_DTSHD_MASTER:
                        case BLURAY_STREAM_TYPE_AUDIO_AC3PLUS_SECONDARY:
                        case BLURAY_STREAM_TYPE_AUDIO_DTSHD_SECONDARY:
                            bd->bluray_info.stream_info[stream_index].type = BLURAY_STREAM_TYPE_AUDIO;
                            break;
                        case BLURAY_STREAM_TYPE_SUB_PG:
                        case BLURAY_STREAM_TYPE_SUB_IG:
                        case BLURAY_STREAM_TYPE_SUB_TEXT:
                            bd->bluray_info.stream_info[stream_index].type = BLURAY_STREAM_TYPE_SUB;
                            break;
                        default:
                            break;
                    }
                    av_strlcpy(bd->bluray_info.stream_info[stream_index].lang, stream.lang, sizeof(LANG));
                    stream_index++;
                }
            }
        }
        if (clpi)
            bd_free_clpi(clpi);

        NAV_MARK_LIST *chap_list = bd_get_chap(bd->bd);
        int chap_count = chap_list->count;
#if LOG_ENABLE
        av_log(h, AV_LOG_INFO, "chap_count(%d)\n", chap_count);
#endif
        if (chap_count > 0) {
            bd->bluray_info.chapter_num = chap_count;
            int size = sizeof(bluray_chapter_info_t) * chap_count;
            bd->bluray_info.chapter_info = (bluray_chapter_info_t *)av_malloc(size);
            memset(bd->bluray_info.chapter_info, 0, size);

            int chap_index;
            for (chap_index = 0; chap_index < chap_count; chap_index++) {
                NAV_MARK mark = chap_list->mark[chap_index];
#if LOG_ENABLE
                av_log(h, AV_LOG_INFO, "chap[%d]number(%d) mark_type(%x) clip_ref(%d) clip_pkt(%d) clip_time(%d) offset(%d) start(%d) duration(%d)\n",
                        chap_index, mark.number, mark.mark_type, mark.clip_ref, mark.clip_pkt, mark.clip_time, (mark.title_pkt * 192L), (mark.title_time * 2 / 90000), (mark.duration * 2 / 90000));
#endif
                bd->bluray_info.chapter_info[chap_index].start = mark.title_time * 2 / 90000;
                bd->bluray_info.chapter_info[chap_index].duration= mark.duration * 2 / 90000;
            }
        }

        ffmpeg_notify(h, PLAYER_EVENTS_BLURAY_INFO, (unsigned long)&bd->bluray_info, 0);
    }

    /* select angle */
    if (bd->angle >= 0) {
        bd_select_angle(bd->bd, bd->angle);
    }

    /* select chapter */
    if (bd->chapter > 1) {
        bd_seek_chapter(bd->bd, bd->chapter - 1);
    }
    h->priv_flags |= FLAGS_LOCALMEDIA;

    return 0;
}

static int bluray_read(URLContext *h, unsigned char *buf, int size)
{
    BlurayContext *bd = h->priv_data;
    int len;

    if (!bd || !bd->bd) {
        return AVERROR(EFAULT);
    }

    len = bd_read(bd->bd, buf, size);

    return len;
}

static int64_t bluray_seek(URLContext *h, int64_t pos, int whence)
{
    BlurayContext *bd = h->priv_data;

    if (!bd || !bd->bd) {
        return AVERROR(EFAULT);
    }

#if LOG_ENABLE
    av_log(h, AV_LOG_INFO, "[%s]pos(%"PRIu64")\n", __FUNCTION__, pos);
    av_log(h, AV_LOG_INFO, "[%s]title size(%"PRIu64")\n", __FUNCTION__, bd_get_title_size(bd->bd));
#endif
    switch (whence) {
    case SEEK_SET:
    case SEEK_CUR:
    case SEEK_END:
        return bd_seek(bd->bd, pos);

    case AVSEEK_SIZE:
        return bd_get_title_size(bd->bd);
    }

    return AVERROR(EINVAL);
}

static int bluray_cmd(URLContext *h, int cmd, int flag, unsigned long info)
{
    if (h == NULL || h->priv_data == NULL) {
        av_log(h, AV_LOG_ERROR, "Failed call :%s\n", __FUNCTION__);
        return -1;
    }
    BlurayContext *bd = h->priv_data;
    if (cmd == 0) {
        av_log(h, AV_LOG_INFO, "[%s]chapter(%d)\n", __FUNCTION__, bd->chapter);
        bd_seek_chapter(bd->bd, bd->chapter);
    }
    return 0;
}

static int bluray_getinfo(URLContext *h, uint32_t  cmd, uint32_t flag, int64_t* info)
{
    if (h == NULL || h->priv_data == NULL) {
        av_log(h, AV_LOG_ERROR, "Failed call :%s\n", __FUNCTION__);
        return -1;
    }
    BlurayContext *bd = h->priv_data;

	switch (cmd) {
		case AVCMD_TOTAL_DURATION:
			*info = bd->duration;
#if LOG_ENABLE
			av_log(h, AV_LOG_INFO, "[%s]get duration(%"PRIu64")\n", __FUNCTION__, bd->duration);
#endif
			break;

		case AVCMD_GET_CLIP_BASE_PCR:
			//*info = bd_get_clip_start_time(bd->bd);
			break;
		default:
			break;
	}

    return 0;
}

static void dump_titles(URLContext *h, BLURAY *bd)
{
    int i;
    int titles_count;
    BLURAY_TITLE_INFO *info = NULL;
    int clip_count = 0;
    int clip_index = 0;
    BLURAY_CLIP_INFO clip;
    int video_stream_count = 0;
    int video_stream_index = 0;
    BLURAY_STREAM_INFO stream_info;
    int audio_stream_count = 0;
    int audio_stream_index = 0;
    int pg_stream_count = 0;
    int pg_stream_index = 0;
    int chapter_count = 0;
    int chapter_index = 0;
    BLURAY_TITLE_CHAPTER chapter;

    titles_count = bd_get_titles(bd, TITLES_RELEVANT, MIN_PLAYLIST_LENGTH);
    av_log(h, AV_LOG_INFO, "titles_count(%d)\n", titles_count);
    for (i = 0; i < titles_count; i++) {
        info = bd_get_title_info(bd, i, 0);
        av_log(h, AV_LOG_INFO, "[%s]title[%d] playlist(%d) duration(%"PRIu64")", __FUNCTION__, i, info->playlist, (info->duration / 90000));
        clip_count = info->clip_count;
        av_log(h, AV_LOG_INFO, "[%s]title[%d] clip_count(%d)", __FUNCTION__, i, clip_count);
        for (clip_index = 0; clip_index < clip_count; clip_index++) {
            clip = info->clips[clip_index];
            av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] pkt_count(%d) still_mode(%d) still_time(%d)", __FUNCTION__, i, clip_index, clip.pkt_count, clip.still_mode, clip.still_time);
            video_stream_count = clip.video_stream_count;
            av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] video_stream_count(%d)", __FUNCTION__, i, clip_index, video_stream_count);
            for (video_stream_index = 0; video_stream_index < video_stream_count; video_stream_index++) {
                stream_info = clip.video_streams[video_stream_index];
                av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] video_streams[%d]", __FUNCTION__, i, clip_index, video_stream_index);
                av_log(h, AV_LOG_INFO, "[%s]coding_type(%x) format(%d) rate(%d) char_code(%d) pid(%d) aspect(%d) subpath_id(%d) lang(%s)",
                        __FUNCTION__, stream_info.coding_type, stream_info.format, stream_info.rate, stream_info.char_code, stream_info.pid, stream_info.aspect, stream_info.subpath_id, stream_info.lang);
            }
            audio_stream_count = clip.audio_stream_count;
            av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] audio_stream_count(%d)", __FUNCTION__, i, clip_index, audio_stream_count);
            for (audio_stream_index = 0; audio_stream_index < audio_stream_count; audio_stream_index++) {
                stream_info = clip.audio_streams[audio_stream_index];
                av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] audio_streams[%d]", __FUNCTION__, i, clip_index, audio_stream_index);
                av_log(h, AV_LOG_INFO, "[%s]coding_type(%x) format(%d) rate(%d) char_code(%d) pid(%d) aspect(%d) subpath_id(%d) lang(%s)",
                        __FUNCTION__, stream_info.coding_type, stream_info.format, stream_info.rate, stream_info.char_code, stream_info.pid, stream_info.aspect, stream_info.subpath_id, stream_info.lang);
            }
            pg_stream_count = clip.pg_stream_count;
            av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] pg_stream_count(%d)", __FUNCTION__, i, clip_index, pg_stream_count);
            for (pg_stream_index = 0; pg_stream_index < pg_stream_count; pg_stream_index++) {
                stream_info = clip.pg_streams[pg_stream_index];
                av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] pg_streams[%d]", __FUNCTION__, i, clip_index, pg_stream_index);
                av_log(h, AV_LOG_INFO, "[%s]coding_type(%x) format(%d) rate(%d) char_code(%d) pid(%d) aspect(%d) subpath_id(%d) lang(%s)",
                        __FUNCTION__, stream_info.coding_type, stream_info.format, stream_info.rate, stream_info.char_code, stream_info.pid, stream_info.aspect, stream_info.subpath_id, stream_info.lang);
            }
            av_log(h, AV_LOG_INFO, "[%s]title[%d] clips[%d] start_time(%"PRIu64") in_time(%"PRIu64") out_time(%"PRIu64")", __FUNCTION__, i, clip_index, (clip.start_time / 90000), (clip.in_time / 90000), (clip.out_time / 90000));

        }
        chapter_count = info->chapter_count;
        av_log(h, AV_LOG_INFO, "[%s]title[%d] chapter_count(%d)", __FUNCTION__, i, chapter_count);
        for (chapter_index = 0; chapter_index < chapter_count; chapter_index++) {
            chapter = info->chapters[chapter_index];
            av_log(h, AV_LOG_INFO, "[%s]title[%d] chapters[%d] idx(%d) start(%"PRIu64") duration(%"PRIu64") offset(%"PRIu64") clip_ref(%d)",
                    __FUNCTION__, i, chapter_index, chapter.idx, (chapter.start / 90000), (chapter.duration / 90000), chapter.offset, chapter.clip_ref);
        }

        bd_free_title_info(info);
    }
}

URLProtocol ff_bluray_protocol = {
    .name            = "bluray",
    .url_close       = bluray_close,
    .url_open        = bluray_open,
    .url_read        = bluray_read,
    .url_seek        = bluray_seek,
    .priv_data_size  = sizeof(BlurayContext),
    .priv_data_class = &bluray_context_class,
    .url_setcmd      = bluray_cmd,
    .url_getinfo     = bluray_getinfo,
};
