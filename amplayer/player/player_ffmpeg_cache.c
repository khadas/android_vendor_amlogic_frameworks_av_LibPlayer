/*
 * player_ffmpeg_cache.c
 *
 * Import PacketQueue From ffmpeg
 *
 * */

#include "player_ffmpeg_cache.h"
#include "player_ffmpeg_ctrl.h"
#include "amconfigutils.h"
#include <cutils/properties.h>

//#include "player_priv.h"

/*============================================
    Macro
============================================
*/

#define PTS_90K 90000
#define PTS_DROP_EDGE (1*PTS_90K)
#define PTS_DISCONTINUE (5*PTS_90K)
#define PTS_DURATION_CALC_TIME (90000*2)
#define CACHE_THREAD_SLEEP_US (10*1000)
#define CURRENT_TIME_MS_DISCONTINUE (2000)

/*============================================
    static function
============================================
*/
static int cache_av_new_packet(AVPacket *dst_pkt, AVPacket *src_pkt);
static int avpkt_cache_get_byindex(av_packet_cache_t *cache_ptr, AVPacket *pkt, int stream_idx);
static int avpkt_cache_check_can_put(av_packet_cache_t *cache_ptr);
static int avpkt_cache_release(av_packet_cache_t *cache_ptr);
static int avpkt_cache_put(void);
static int avpkt_cache_init(av_packet_cache_t *cache_ptr, void *context);
static int64_t avpkt_cache_queue_search(PacketQueue *q, int64_t seekTimeSec);
static int64_t avpkt_cache_queue_search_bypts(PacketQueue *q, int64_t pts);
static int avpkt_cache_queue_seek(PacketQueue *q, int64_t seekTimeMs);
static int avpkt_cache_queue_seek_bypts(PacketQueue *q, int64_t pts, int small_flag);
static int avpkt_cache_queue_seektoend(PacketQueue *q);
static int avpkt_cache_check_netlink(void);
static int avpkt_cache_check_frames_reseved_enough(av_packet_cache_t *cache_ptr);
static int packet_update_bufed_time(void);
static int avpkt_cache_queue_check_backlevel(PacketQueue *q);
static int avpkt_cache_queue_dropframes(PacketQueue *q, int64_t dst_pts, int small_flag);
static int avpkt_cache_interrupt_read(av_packet_cache_t *cache_ptr);
static int avpkt_cache_uninterrupt_read(av_packet_cache_t * cache_ptr);
static int avpkt_cache_avsync(av_packet_cache_t *cache_ptr);

/*============================================
    static var
============================================
*/
static av_packet_cache_t s_avpkt_cache;

#ifdef DEBUT_PUT_GET
static int64_t get_cnt = 0;
static int64_t put_cnt = 0;
#endif

/*============================================
    function realize
============================================
*/
static int cache_av_new_packet(AVPacket *dst_pkt, AVPacket *src_pkt)
{
    uint8_t *data = NULL;
    int ret = 0;
    int i = 0;

    if ((unsigned)src_pkt->size < (unsigned)src_pkt->size + FF_INPUT_BUFFER_PADDING_SIZE) {
        data = av_malloc(src_pkt->size + FF_INPUT_BUFFER_PADDING_SIZE);
    }

    if (data) {
        memset(data + src_pkt->size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    } else {
        log_print("%s no mem", __FUNCTION__);
        return -1;
    }

    dst_pkt->pts   = src_pkt->pts;
    dst_pkt->dts   = src_pkt->dts;
    dst_pkt->pos   = src_pkt->pos;
    dst_pkt->duration = src_pkt->duration;
    dst_pkt->convergence_duration = src_pkt->convergence_duration;
    dst_pkt->flags = src_pkt->flags;
    dst_pkt->stream_index = src_pkt->stream_index;
    dst_pkt->side_data       = NULL;
    dst_pkt->side_data_elems = 0;
    dst_pkt->data = data;
    dst_pkt->size = src_pkt->size;
    dst_pkt->destruct = av_destruct_packet;

    memcpy(dst_pkt->data, src_pkt->data, src_pkt->size);
    if (src_pkt->side_data_elems > 0) {
        dst_pkt->side_data = av_malloc(src_pkt->side_data_elems * sizeof(*src_pkt->side_data));
        if (dst_pkt->side_data == NULL) {
            goto failed_alloc;
        }
        memcpy(dst_pkt->side_data, src_pkt->side_data, src_pkt->side_data_elems * sizeof(*src_pkt->side_data));
        memset(dst_pkt->side_data, 0, src_pkt->side_data_elems * sizeof(*src_pkt->side_data));
        for (i = 0; i < src_pkt->side_data_elems; i++) {
            if ((unsigned)(src_pkt->side_data[i].size) > (unsigned)(src_pkt->side_data[i].size) + FF_INPUT_BUFFER_PADDING_SIZE) {
                goto failed_alloc;
            }
            dst_pkt->side_data[i].data = av_malloc((unsigned)(src_pkt->side_data[i].size) + FF_INPUT_BUFFER_PADDING_SIZE);
            memcpy(dst_pkt->side_data[i].data, src_pkt->side_data[i].data, src_pkt->side_data[i].size);
            memcpy(dst_pkt->side_data[i].data, src_pkt->side_data[i].data, src_pkt->side_data[i].size);
        }
    }

    return 0;
failed_alloc:
    av_destruct_packet(dst_pkt);
    return -1;
}

static int close_to(int a, int b, int m)
{
    return (abs(a - b) < m) ? 1 : 0;
}

#define RATE_CORRECTION_THRESHOLD 90
#define RATE_24_FPS  3750   /* 24.04  pts*/
#define RATE_25_FPS  3600   /* 25 */
#define RATE_26_FPS  3461   /* 26 */
#define RATE_30_FPS  3000   /*30*/
#define RATE_50_FPS  1800   /*50*/
#define RATE_60_FPS  1500   /*60*/

static int duration_pts_invalid_check(int pts_duration)
{
    int fm_duration = 0;

    if (pts_duration < 0) {
        return 0;
    }

    if (close_to(pts_duration,
                 RATE_24_FPS,
                 RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_24_FPS;
    } else if (close_to(pts_duration,
                        RATE_25_FPS,
                        RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_25_FPS;
    } else if (close_to(pts_duration,
                        RATE_26_FPS,
                        RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_26_FPS;
    } else if (close_to(pts_duration,
                        RATE_30_FPS,
                        RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_30_FPS;
    } else if (close_to(pts_duration,
                        RATE_50_FPS,
                        RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_50_FPS;
    } else if (close_to(pts_duration,
                        RATE_60_FPS,
                        RATE_CORRECTION_THRESHOLD) == 1) {
        fm_duration = RATE_60_FPS;
    }

    return fm_duration;

}

static int packet_queue_put_update(PacketQueue *q, AVPacket *pkt)
{
    int ret = -1;
    MyAVPacketList *pkt1;
    MyAVPacketList *pkttmp;
    int64_t diff_pts = 0;
    int64_t avg_duration_ms = 0;
    int64_t cache_pts;
    int64_t discontinue_pts = 0;
    int64_t bak_cache_pts = 0;
    int discontinue_flag = 0;

    if (s_avpkt_cache.state != 2) {
        return -2;
    }

    cache_lock(&q->lock);
    if (q->nb_packets == q->max_packets) {
        if (q->queue_max_kick == 0) {
            q->queue_max_kick = 1;
            q->queue_maxtime_pts = q->bak_cache_pts; //pts value
            log_print("%d reach maxpackets, mem:%d, firstpts:0x%llx, lastpts:0x%llx, diff_pts_ms:%lld",
                      pkt->stream_index, q->size, q->head_valid_pts, q->tail_valid_pts,
                      (int64_t)((q->tail_valid_pts - q->head_valid_pts) / 90));
        }

        //delete frist_pkt
        pkttmp = q->first_pkt;
        q->first_pkt = pkttmp->next;
        q->first_pkt->priv = NULL;
        pkttmp->priv = NULL;

        q->backwardsize -= pkttmp->pkt.size;
        //calc free mem for this packet
        q->size -= pkttmp->pkt.size;
        {
            if (pkttmp->pkt.side_data_elems > 0) {
                q->size -= (pkttmp->pkt.side_data_elems * sizeof(pkttmp->pkt.side_data));
            }
        }
        q->size -= sizeof(*pkttmp);
        //end

        av_free_packet(&pkttmp->pkt);
        av_free(pkttmp);
        pkttmp = NULL;
        //q->nb_packets--;

        q->frames_for_seek_backward--;

        //update queue first pts
        pkttmp = q->first_pkt;
        while (pkttmp != NULL) {
            if (pkttmp->pkt.pts != AV_NOPTS_VALUE) {
                q->head_valid_pts = pkttmp->pkt.pts;
                break;
            }
            pkttmp = pkttmp->next;
        }
    }

    /*
    from first pkt, check video keyframe, if not drop it, until max(61frames)
    */
    if (q->first_keyframe == -1) {
        log_print("pkt[%d]-pts:%llx\n", pkt->stream_index, pkt->pts);
        if (pkt->flags & AV_PKT_FLAG_KEY) {
            q->first_keyframe = 1;
            q->first_keyframe_pts = pkt->pts;
            q->dropref_pts = pkt->pts;
            if (pkt->stream_index == s_avpkt_cache.video_index) {
                log_print("find first video keyframe in %d frames, key_pts:0x%llx", q->keyframe_check_cnt, pkt->pts);
            } else if (pkt->stream_index == s_avpkt_cache.audio_index) {
                log_print("find first audio keyframe in %d frames, key_pts:0x%llx", q->keyframe_check_cnt, pkt->pts);
            }
            q->keyframe_check_cnt = 0;
        } else {
            q->keyframe_check_cnt++;

            if (q->keyframe_check_cnt >= q->keyframe_check_max) {
                q->first_keyframe = 0;//not found first keyframe
                q->dropref_pts = q->first_valid_pts;
                if (pkt->stream_index == s_avpkt_cache.video_index) {
                    log_print("not find first video keyframe in %d frames", q->keyframe_check_cnt);
                } else if (pkt->stream_index == s_avpkt_cache.audio_index) {
                    log_print("not find first audio keyframe in %d frames", q->keyframe_check_cnt);
                }

                q->keyframe_check_cnt = 0;
            } else {
                if (pkt->stream_index == s_avpkt_cache.video_index) {
                    //return -2;
                }
            }
        }
    }

    pkt1 = av_malloc(sizeof(MyAVPacketList));
    if (!pkt1) {
        log_print("[%s]no mem, nb_packets:%d, max_packets(%d)\n", __FUNCTION__, q->nb_packets, q->max_packets);
        if (q->nb_packets > 0) {
            q->max_packets = q->nb_packets;
        }
        log_print("[%s]no mem, change max_packets to nb_packets(%d)\n", __FUNCTION__, q->max_packets);
        cache_unlock(&q->lock);
        return -1;
    }

    //add to tail
    if ((ret = cache_av_new_packet(&pkt1->pkt, pkt)) < 0) {
        av_free(pkt1);
        cache_unlock(&q->lock);
        return ret;
    }
    pkt1->next = NULL;
    pkt1->used = 0;
    pkt1->frame_id = s_avpkt_cache.read_frames + 1;
    pkt1->pts = pkt1->pkt.pts == AV_NOPTS_VALUE ? pkt1->pkt.dts : pkt1->pkt.pts;
    //log_print("in id:%lld idx:%d, pts2:0x%llx",pkt1->frame_id,pkt1->pkt.stream_index,pkt1->pts);

    /*
        calc frame_dur, trust keyframe pts
    */
    int frame_dur = 0;
    int frame_dur_pts = 0;
    if (q->dur_calc_done == 0) {
        //find first keyframe pts
        if (pkt->stream_index == s_avpkt_cache.video_index) {
            if ((pkt->flags & AV_PKT_FLAG_KEY) && pkt->pts != AV_NOPTS_VALUE) {
                //q->dur_calc_cnt++;
                if (q->dur_calc_pts_start == -1) {
                    q->dur_calc_pts_start = pkt->pts;
                    q->dur_calc_pts_end = pkt->pts;
                    q->dur_calc_cnt = 1;//0->1
                } else {
                    q->dur_calc_pts_end = pkt->pts;
                    if ((q->dur_calc_pts_end - q->dur_calc_pts_start) >= PTS_DURATION_CALC_TIME) {
                        //keep first keyframe vpts
                        if (q->first_keyframe_pts == -1) {
                            q->first_keyframe_pts = pkt->pts;
                        }

                        //calce pts enough
                        frame_dur = (int)((q->dur_calc_pts_end - q->dur_calc_pts_start) / q->dur_calc_cnt);
                        frame_dur_pts = duration_pts_invalid_check(frame_dur);
                        if (frame_dur_pts > 0) {
                            //q->frame_dur_pts = frame_dur_pts;
                            q->frame_dur_pts = frame_dur;
                            //if (pkt1->pkt.stream_index == s_avpkt_cache.video_index)
                            //log_print("success idx:%d, frame_dur_pts :%d, frame_dur:%d, pts1:0x%llx, pts2:0x%llx, cnt:%d",
                            //pkt1->pkt.stream_index, q->frame_dur_pts, frame_dur, q->dur_calc_pts_start, q->dur_calc_pts_end, q->dur_calc_cnt);
                            q->dur_calc_cnt = 1;
                            q->dur_calc_pts_start = q->dur_calc_pts_end;
                        } else {
                            //if (pkt1->pkt.stream_index == s_avpkt_cache.video_index)
                            //log_print("fail idx:%d, frame_dur_pts :%d, frame_dur:%d, pts1:0x%llx, pts2:0x%llx, cnt:%d",
                            //pkt1->pkt.stream_index, q->frame_dur_pts, frame_dur, q->dur_calc_pts_start, q->dur_calc_pts_end, q->dur_calc_cnt);
                            q->dur_calc_pts_start = q->dur_calc_pts_end;
                            q->dur_calc_cnt = 1;
                        }
                    }
                }
            }

            if (q->dur_calc_cnt > 0) {
                q->dur_calc_cnt++;
            }
        }
    }
    //end

    discontinue_flag = q->discontinue_flag;
    discontinue_pts = q->discontinue_pts;
    cache_pts = q->cache_pts;
    bak_cache_pts = q->bak_cache_pts;

    if (pkt1->pkt.pts != AV_NOPTS_VALUE) {
        if (q->frames_in <= 60 || q->trust_keyframe == 0 || (pkt->flags & AV_PKT_FLAG_KEY)) {
            if (q->pts1 == -1) {
                q->pts1 = pkt1->pkt.pts;
                q->pts2 = pkt1->pkt.pts;
            } else {
                q->pts1 = q->pts2;
                q->pts2 = pkt1->pkt.pts;
                q->last_pts2 = q->pts2;
            }

            if (q->trust_keyframe == 1 && (q->pts2 - q->pts1) >= 2 * 90000) {
                q->trust_keyframe = 0;
            }

            diff_pts = q->pts2 - q->pts1;

#if 0
            if (pkt1->pkt.stream_index == s_avpkt_cache.video_index) {
                if (pkt->flags & AV_PKT_FLAG_KEY) {
                    log_print("video frame-key:0x%llx, dts:0x%llx,  diff_pts:0x%llx", pkt1->pkt.pts, pkt1->pkt.dts, diff_pts);
                } else {
                    log_print("video frame:0x%llx, dts:0x%llx,  diff_pts:0x%llx", pkt1->pkt.pts, pkt1->pkt.dts, diff_pts);
                }
            } else if (pkt1->pkt.stream_index == s_avpkt_cache.audio_index) {
                if (pkt->flags & AV_PKT_FLAG_KEY) {
                    log_print("audio frame-key:0x%llx, diff_pts:0x%llx", pkt1->pkt.pts, diff_pts);
                } else {
                    //log_print("audio frame:0x%llx, diff_pts:0x%llx", pkt1->pkt.pts, diff_pts);
                }
            }
#endif
            if (abs(diff_pts) >= PTS_DISCONTINUE /*|| diff_pts < 0*/) {
                if (discontinue_flag == 0) {
                    discontinue_flag = 1;
                    discontinue_pts = diff_pts;
                    cache_pts += discontinue_pts;
                } else {
                    discontinue_flag = 1; //连续两次discontinue
                    cache_pts -= discontinue_pts;
                    if (q->frame_dur_pts > 0) {
                        cache_pts += q->frame_dur_pts;
                    }
                    discontinue_pts = diff_pts;
                    cache_pts += discontinue_pts;
                }
            } else {
                if (discontinue_flag == 0) {
                    cache_pts += diff_pts;
                    bak_cache_pts = cache_pts;
                    if (q->queue_max_kick == 1) {
                        q->firstPkt_playtime_pts += diff_pts;
                    }
                } else {
                    discontinue_flag = 0;
                    cache_pts -= discontinue_pts;
                    if (q->frame_dur_pts > 0) {
                        cache_pts += q->frame_dur_pts;
                    }
                    cache_pts += diff_pts;
                    bak_cache_pts = cache_pts;
                    discontinue_pts = 0;
                }
            }
        } else {
            if (q->frames_in >= 61
                && (q->pts2 == q->last_pts2)
                && q->trust_keyframe == 1) {
                q->trust_keyframe = 0;
                if (q->frame_dur_pts > 0) {
                    bak_cache_pts += (q->frame_dur_pts * q->frames_in);
                }
            }
        }
    }

    q->discontinue_flag = discontinue_flag;
    q->discontinue_pts = discontinue_pts;
    q->cache_pts = cache_pts;
    q->bak_cache_pts = bak_cache_pts;
    pkt1->offset_pts = bak_cache_pts;

    if (!q->last_pkt) {
        pkt1->priv = NULL;
        q->first_pkt = pkt1;
        q->cur_pkt = q->first_pkt;
        if (q->cur_pkt->pkt.pts != AV_NOPTS_VALUE) {
            q->cur_valid_pts = q->cur_pkt->pkt.pts;
        }
    } else {
        pkt1->priv = q->last_pkt;
        q->last_pkt->next = pkt1;
    }

    if (q->cur_pkt == q->last_pkt && q->cur_pkt->used == 1) {
        q->cur_pkt = pkt1;
        q->curPkt_playtime_pts = pkt1->offset_pts;
    }


    q->last_pkt = pkt1;
    if (q->nb_packets < q->max_packets) {
        q->nb_packets++;
    }

    //calc mem used for this packet
    q->size += pkt1->pkt.size;
    {
        if (pkt->side_data_elems > 0) {
            q->size += (pkt1->pkt.side_data_elems * sizeof(pkt1->pkt.side_data));
        }
    }
    q->size += sizeof(*pkt1);
    //end

    if (q->head_valid_pts == -1 && pkt1->pkt.pts != AV_NOPTS_VALUE) {
        q->head_valid_pts = pkt1->pkt.pts;
        q->first_valid_pts = pkt1->pkt.pts;
    }

    q->frames_in++;

    {
        q->forwardsize += pkt1->pkt.size;
        q->frames_for_seek_forward++;
    }

    q->last_pkt = pkt1;

    if (q->last_pkt->pkt.pts != AV_NOPTS_VALUE && pkt1->pkt.pts != AV_NOPTS_VALUE) {
        q->tail_valid_pts = pkt1->pkt.pts;
    }

    if (q->firstPkt_playtime_pts == -1) {
        q->firstPkt_playtime_pts = q->bak_cache_pts;
        log_print("firstPkt_playtime_offset_pts:0x%llx, curPkt_playtime_offset_pts:0x%llx, bak_cache_pts:%lld",
                  q->firstPkt_playtime_pts, q->cur_pkt->offset_pts, q->bak_cache_pts);
    }

    q->lastPkt_playtime_pts = q->bak_cache_pts;
    cache_unlock(&q->lock);
    return 0;
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q, av_packet_cache_t *cache_ptr, int stream_index)
{
    memset(q, 0, sizeof(PacketQueue));
    cache_lock_init(&q->lock, NULL);

    q->stream_index = stream_index;
    if (stream_index == cache_ptr->audio_index) {
        q->max_packets = cache_ptr->audio_max_packet;
    } else if (stream_index == cache_ptr->video_index) {
        q->max_packets = cache_ptr->video_max_packet;
    } else if (stream_index == cache_ptr->sub_index) {
        q->max_packets = cache_ptr->sub_max_packet;
    }

    q->size = 0;
    q->backwardsize = 0;
    q->forwardsize = 0;
    q->nb_packets = 0;

    q->first_pkt = NULL;
    q->cur_pkt = NULL;
    q->last_pkt = NULL;

    q->first_valid_pts = -1;
    q->head_valid_pts = -1;
    q->tail_valid_pts = -1;
    q->cur_valid_pts = -1;

    q->pts1 = -1;
    q->pts2 = -1;
    q->last_pts2 = -1;
    q->dur_calc_pts_start = -1;
    q->dur_calc_pts_end = -1;
    q->dur_calc_cnt = 0;
    q->dur_calc_done = 0;
    q->cache_pts = 0;
    q->bak_cache_pts = 0;
    q->discontinue_flag = 0;
    q->discontinue_pts = 0;
    q->frame_dur_pts = 0;

    q->firstPkt_playtime_pts = -1;
    q->curPkt_playtime_pts = -1;
    q->lastPkt_playtime_pts = 0;

    q->queue_max_kick = 0;
    q->queue_maxtime_pts = 0;

    q->frames_in = 0;
    q->frames_out = 0;

    q->first_keyframe_pts = -1;
    q->trust_keyframe = 1;
    q->dropref_pts = -1;
    q->first_keyframe = -1;
    q->keyframe_check_cnt = 0;
    q->keyframe_check_max = 61;

    q->frames_backward_level = (float)am_getconfig_int_def("libplayer.cache.backseek", 1) / 1000;
    q->frames_max_seekbackword = (int)(q->frames_backward_level * q->max_packets);
    if (q->frames_max_seekbackword == 0) {
        q->frames_max_seekbackword = 1;
    }
    q->frames_max_seekforword = (q->max_packets - q->frames_for_seek_backward);
    q->frames_for_seek_forward = 0;
    q->frames_for_seek_backward = 0;
}

static void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    cache_lock(&q->lock);

    log_print("%s nb_packets:%d\n", __FUNCTION__, q->nb_packets);
    int i = 0;
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_free(pkt);
        i++;
    }

    q->first_pkt = NULL;
    q->cur_pkt = NULL;
    q->last_pkt = NULL;

    q->nb_packets = 0;
    q->size = 0;
    q->backwardsize = 0;
    q->forwardsize = 0;

    q->first_valid_pts = -1;
    q->head_valid_pts = -1;
    q->tail_valid_pts = -1;
    q->cur_valid_pts = -1;

    q->pts1 = -1;
    q->pts2 = -1;
    q->last_pts2 = -1;
    q->dur_calc_pts_start = -1;
    q->dur_calc_pts_end = -1;
    q->dur_calc_cnt = 0;
    q->dur_calc_done = 0;
    q->discontinue_pts = 0;
    q->discontinue_flag = 0;
    q->cache_pts = 0;
    q->bak_cache_pts = 0;

    q->firstPkt_playtime_pts = -1;
    q->curPkt_playtime_pts = -1;
    q->lastPkt_playtime_pts = -1;

    q->queue_maxtime_pts = 0;
    q->queue_max_kick = 0;

    q->frames_in = 0;
    q->frames_out = 0;

    q->trust_keyframe = 1;
    q->first_keyframe_pts = -1;
    q->dropref_pts = -1;
    q->keyframe_check_cnt = 0;
    q->first_keyframe = -1;

    q->frames_for_seek_forward = 0;
    q->frames_for_seek_backward = 0;

    cache_unlock(&q->lock);

}

static void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);
    cache_lock_uninit(&q->lock);
}

/*
call pre-condition:
    can get, cur_pkt != NULL
*/
int avpkt_cache_queue_get(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;
    int ret = 0;
    int64_t pts2 = 0;
    int64_t pts1 = 0;

    if (!q || !pkt) {
        log_print("%s invalid param\n", __FUNCTION__);
        return -1;
    }

    cache_lock(&q->lock);


    pkt1 = q->cur_pkt;
    if (pkt1 && pkt1->used == 0) {
        if (pkt1->pkt.pts != AV_NOPTS_VALUE) {
            q->cur_valid_pts = pkt1->pkt.pts;
        }

        q->frames_for_seek_forward--;
        q->frames_for_seek_backward++;
        q->frames_out++;
        q->backwardsize += pkt1->pkt.size;
        q->forwardsize -= pkt1->pkt.size;
        *pkt = pkt1->pkt;
        pkt->destruct = NULL;
        //log_print("out id:%lld idx:%d, pts2:0x%llx, first id:%lld",pkt1->frame_id,pkt1->pkt.stream_index,pkt1->pts, q->first_pkt->frame_id);

        pkt1->used = 1;
        if (q->cur_pkt->next != NULL) {
            q->cur_pkt = q->cur_pkt->next;
        }
        q->curPkt_playtime_pts = q->cur_pkt->offset_pts;
    }

    cache_unlock(&q->lock);

    return ret;
}

static int avpkt_cache_init(av_packet_cache_t *cache_ptr, void *context)
{
    play_para_t *player = (play_para_t *)context;

    memset(cache_ptr, 0x0, sizeof(av_packet_cache_t));
    if (player->pFormatCtx->pb) {
        cache_ptr->local_play = player->pFormatCtx->pb->local_playback;
    }
    cache_ptr->audio_max_packet = am_getconfig_int_def("libplayer.cache.amaxframes", 7000);
    cache_ptr->video_max_packet = am_getconfig_int_def("libplayer.cache.vmaxframes", 3500);
    cache_ptr->sub_max_packet = am_getconfig_int_def("libplayer.cache.smaxframes", 1000);
    if (cache_ptr->local_play) {
        cache_ptr->max_cache_mem = 60 * 1024 * 1024;
    } else {
        cache_ptr->max_cache_mem = am_getconfig_int_def("libplayer.cache.maxmem", 67108864);
    }
    cache_ptr->enable_seek_in_cache = am_getconfig_int_def("libplayer.cache.seekenable", 0);

    cache_ptr->has_audio = player->astream_info.has_audio;
    cache_ptr->has_video = player->vstream_info.has_video;
    cache_ptr->has_sub   = player->sstream_info.has_sub;

    cache_ptr->audio_index = player->astream_info.audio_index;
    cache_ptr->video_index = player->vstream_info.video_index;
    cache_ptr->sub_index   = player->sstream_info.sub_index;

    cache_ptr->first_apts = -1;
    cache_ptr->first_vpts = -1;
    cache_ptr->first_spts = -1;
    cache_ptr->seekTimeMs = 0;

    if (player->playctrl_info.time_point > 0) {
        cache_ptr->starttime_ms = (int64_t)(player->playctrl_info.time_point * 1000);
    } else {
        if (player->state.current_ms > 0) {
            cache_ptr->starttime_ms = (int64_t)(player->state.current_ms);
        } else {
            cache_ptr->starttime_ms = 0;
        }
    }

    cache_ptr->read_frames = 0;
    cache_ptr->video_cachems = 0;
    cache_ptr->audio_cachems = 0;
    cache_ptr->sub_cachems = 0;
    cache_ptr->currenttime_ms = 0;
    cache_ptr->last_currenttime_ms = 0;

    cache_ptr->discontinue_current_ms = 0;
    cache_ptr->discontinue_current_ms_flag = 0;
    cache_ptr->discontinue_current_ms_checked = 0;

    cache_ptr->netdown = 0;
    cache_ptr->last_netdown_state = 0;

    if (cache_ptr->has_audio) {
        packet_queue_init(&cache_ptr->queue_audio, cache_ptr, cache_ptr->audio_index);
        cache_ptr->queue_audio.timebase = player->astream_info.audio_duration;
    }
    if (cache_ptr->has_video) {
        packet_queue_init(&cache_ptr->queue_video, cache_ptr, cache_ptr->video_index);
        cache_ptr->queue_video.timebase = player->vstream_info.video_pts;
    }
    if (cache_ptr->has_sub) {
        packet_queue_init(&cache_ptr->queue_sub, cache_ptr, cache_ptr->sub_index);
    }
    cache_ptr->context = context;

    cache_ptr->state = 1;
    cache_ptr->error = 0;

    cache_ptr->enable_keepframes = am_getconfig_int_def("libplayer.cache.keepframe_en", 0);
    if (cache_ptr->enable_keepframes == 1) {
        cache_ptr->resetkeepframes = 0;
        cache_ptr->keepframes = am_getconfig_int_def("libplayer.cache.keepframes", 125);
        cache_ptr->enterkeepframes = am_getconfig_int_def("libplayer.cache.enterkeepframes", 300);
        cache_ptr->startenterkeepframes = 0;
    }

    cache_ptr->start_avsync_finish = 0;
    cache_ptr->need_avsync = 0;//default no need av seek when do start
    cache_ptr->search_by_keyframe = am_getconfig_int_def("libplayer.cache.seekbykeyframe", 1);//default seek by keyframe

    cache_ptr->trickmode = 0;
    cache_ptr->fffb_start = 0;
    cache_ptr->fffb_out_frames = am_getconfig_int_def("libplayer.cache.fffbframes", 61);

#ifdef DEBUT_PUT_GET
    get_cnt = 0;
    put_cnt = 0;
#endif

    log_print("[%s:%d]has_audio:%d,aidx:%d,  has_video:%d,vidx:%d, has_sub:%d,sidx:%d\n",
              __FUNCTION__, __LINE__, cache_ptr->has_audio, cache_ptr->audio_index,
              cache_ptr->has_video, cache_ptr->video_index,
              cache_ptr->has_sub, cache_ptr->sub_index);

    return 0;
}

static int avpkt_cache_get_byindex(av_packet_cache_t *cache_ptr, AVPacket *pkt, int stream_idx)
{
    if (cache_ptr == NULL || pkt == NULL) {
        log_print("%s:%d invalid param", __FUNCTION__, __LINE__);
        return -1;
    }

    int ret = -1;
    if (stream_idx == cache_ptr->audio_index) {
        ret = avpkt_cache_queue_get(&cache_ptr->queue_audio, pkt);
    } else if (stream_idx == cache_ptr->video_index) {
        ret = avpkt_cache_queue_get(&cache_ptr->queue_video, pkt);
    } else if (stream_idx == cache_ptr->sub_index) {
        ret = avpkt_cache_queue_get(&cache_ptr->queue_sub, pkt);
    }

    return ret;
}

static int printcnt = 0;
int avpkt_cache_put_update(av_packet_cache_t *cache_ptr, AVPacket *pkt)
{
    int ret = -1;
    play_para_t *player = (play_para_t *)cache_ptr->context;
    if (cache_ptr->has_audio && pkt->stream_index == cache_ptr->audio_index) {
        ret = packet_queue_put_update(&cache_ptr->queue_audio, pkt);
        if (ret < 0) {
            return ret;
        }
        cache_ptr->audio_count = cache_ptr->queue_audio.nb_packets;
        cache_ptr->audio_size = cache_ptr->queue_audio.size;
        cache_ptr->audio_cachems = (int64_t)(cache_ptr->queue_audio.bak_cache_pts / 90);
        if (cache_ptr->first_apts == -1 && cache_ptr->queue_audio.first_valid_pts != -1) {
            cache_ptr->first_apts = cache_ptr->queue_audio.first_valid_pts;
        }

        if (0/*printcnt%5 == 0*/)
            log_print("aidx:%d, nb_pkts:%d, canread:%d, firstpts:%llx, cur_pktpts:%llx, playpts:0x%x, lastpts:%llx, ret:%d\n",
                      cache_ptr->audio_index, cache_ptr->queue_audio.nb_packets, cache_ptr->queue_audio.frames_for_seek_forward,
                      cache_ptr->queue_audio.head_valid_pts, cache_ptr->queue_audio.cur_valid_pts, player->state.current_pts, cache_ptr->queue_audio.tail_valid_pts, ret);
        if (printcnt == 100000) {
            printcnt = 0;
        } else {
            printcnt++;
        }
    } else if (cache_ptr->has_video && pkt->stream_index == cache_ptr->video_index) {
        ret = packet_queue_put_update(&cache_ptr->queue_video, pkt);
        if (ret < 0) {
            return ret;
        }
        cache_ptr->video_count = cache_ptr->queue_video.nb_packets;
        cache_ptr->video_size = cache_ptr->queue_video.size;
        cache_ptr->video_cachems = (int64_t)(cache_ptr->queue_video.bak_cache_pts / 90);
        if (cache_ptr->first_vpts == -1 && cache_ptr->queue_video.first_valid_pts != -1) {
            cache_ptr->first_vpts = cache_ptr->queue_video.first_valid_pts;
        }

        if (0/*printcnt%5 == 0*/)
            log_print("vidx:%d, nb_pkts:%d, canread:%d, firstpts:%llx, cur_pktpts:%llx, playpts:0x%x, lastpts:%llx, ret:%d\n",
                      cache_ptr->video_index, cache_ptr->queue_video.nb_packets, cache_ptr->queue_video.frames_for_seek_forward,
                      cache_ptr->queue_video.head_valid_pts, cache_ptr->queue_video.cur_valid_pts, player->state.current_pts, cache_ptr->queue_video.tail_valid_pts, ret);
        if (printcnt == 100000) {
            printcnt = 0;
        } else {
            printcnt++;
        }
    } else if (cache_ptr->has_sub && pkt->stream_index == cache_ptr->sub_index) {
        ret = packet_queue_put_update(&cache_ptr->queue_sub, pkt);
        if (ret < 0) {
            return ret;
        }
        cache_ptr->sub_count = cache_ptr->queue_sub.nb_packets;
        cache_ptr->sub_size = cache_ptr->queue_sub.size;
        cache_ptr->sub_cachems = (int64_t)(cache_ptr->queue_sub.bak_cache_pts / 90);
        if (cache_ptr->first_spts == -1 && cache_ptr->queue_sub.first_valid_pts != -1) {
            cache_ptr->first_spts = cache_ptr->queue_sub.first_valid_pts;
        }

        if (0)
            log_print("sidx:%d, nb_pkts:%d, canread:%d, firstpts:%llx, cur_pktpts:%llx, playpts:0x%x, lastpts:%llx, ret:%d\n",
                      cache_ptr->sub_index, cache_ptr->queue_sub.nb_packets, cache_ptr->queue_sub.frames_for_seek_forward,
                      cache_ptr->queue_sub.head_valid_pts, cache_ptr->queue_sub.cur_valid_pts, player->state.current_pts, cache_ptr->queue_sub.tail_valid_pts, ret);
    } else {
    }

    return 0;
}

int avpkt_cache_check_can_get(av_packet_cache_t *cache_ptr, int* stream_idx)
{
    int64_t audio_cur_pkt_frame_id = -1;
    int64_t video_cur_pkt_frame_id = -1;
    int64_t sub_cur_pkt_frame_id = -1;
    int64_t small_frame_id = -1;

    if (cache_ptr->has_video
        && cache_ptr->queue_video.frames_for_seek_forward > 0) {
        video_cur_pkt_frame_id = cache_ptr->queue_video.cur_pkt->frame_id;
        small_frame_id = video_cur_pkt_frame_id;
        *stream_idx = cache_ptr->video_index;
    }

    if (cache_ptr->trickmode == 0) {
        if (cache_ptr->has_audio
            && cache_ptr->queue_audio.frames_for_seek_forward > 0) {
            audio_cur_pkt_frame_id = cache_ptr->queue_audio.cur_pkt->frame_id;
            if (small_frame_id == -1) {
                small_frame_id = audio_cur_pkt_frame_id;
                *stream_idx = cache_ptr->audio_index;
            } else {
                if (audio_cur_pkt_frame_id < small_frame_id) {
                    small_frame_id = audio_cur_pkt_frame_id;
                    *stream_idx = cache_ptr->audio_index;
                }
            }
        }

        if (cache_ptr->has_sub
            && cache_ptr->queue_sub.frames_for_seek_forward > 0) {
            sub_cur_pkt_frame_id = cache_ptr->queue_sub.cur_pkt->frame_id;
            if (small_frame_id == -1) {
                small_frame_id = sub_cur_pkt_frame_id;
                *stream_idx = cache_ptr->sub_index;
            } else {
                if (sub_cur_pkt_frame_id < small_frame_id) {
                    small_frame_id = sub_cur_pkt_frame_id;
                    *stream_idx = cache_ptr->sub_index;
                }
            }
        }
    }

    if (*stream_idx < 0 && (cache_ptr->queue_audio.frames_for_seek_forward > 0 || cache_ptr->queue_video.frames_for_seek_forward > 0)) {
        log_print("idx:%d,a_nb:%d, acread:%d, v_nb:%d, vcanread:%d",
                  *stream_idx, cache_ptr->queue_audio.nb_packets, cache_ptr->queue_audio.frames_for_seek_forward,
                  cache_ptr->queue_video.nb_packets, cache_ptr->queue_video.frames_for_seek_forward
                 );
    }

    return (*stream_idx < 0 ? 0 : 1);
}

/*
return :
1-back level enough, can put
0-back level not enough, can not put
*/
static int avpkt_cache_queue_check_backlevel(PacketQueue *q)
{
    play_para_t *player = (play_para_t *)s_avpkt_cache.context;

    int64_t cache_max_pts = q->lastPkt_playtime_pts - q->firstPkt_playtime_pts;
    int64_t playtime_pts =
        (int64_t)(((int64_t)(player->state.current_ms) - (s_avpkt_cache.starttime_ms + s_avpkt_cache.discontinue_current_ms)) * 90);
    int64_t play_to_first_pts = playtime_pts - q->firstPkt_playtime_pts;

    if (q->firstPkt_playtime_pts < q->curPkt_playtime_pts
        && q->curPkt_playtime_pts <= q->lastPkt_playtime_pts
        && (play_to_first_pts <= (int64_t)(cache_max_pts * q->frames_backward_level))) {
        return 0;
    }
    s_avpkt_cache.last_currenttime_ms = (int64_t)(player->state.current_ms);
    return 1;
}
int avpkt_cache_queue_check_can_put(PacketQueue *q, int64_t current_ms)
{
    int ret = 0;
    int64_t firstPkt_ms = 0;
    int64_t curPkt_ms = 0;
    int64_t lastPkt_ms = 0;

    cache_lock(&q->lock);

    if (q->frames_backward_level > 0.0) {
        if (q->frames_for_seek_backward > q->frames_max_seekbackword
            && avpkt_cache_queue_check_backlevel(q) == 1) {
            ret = 1;
        } else {
            ret = 0;
        }
    } else {
        if (q->frames_for_seek_backward > 1
            && q->frames_for_seek_backward <= q->max_packets) {
            ret = 1;
        } else {
            ret = 0;
        }
    }

    cache_unlock(&q->lock);
    return ret;
}

static int avpkt_cache_check_can_put(av_packet_cache_t *cache_ptr)
{
    play_para_t *player = (play_para_t *)cache_ptr->context;
    int64_t current_ms = (int64_t)(player->state.current_ms);
    int vcanput = 0;
    int acanput = 0;
    int ret = 0;

    int avpkt_total_mem;

    avpkt_total_mem = cache_ptr->queue_audio.size + cache_ptr->queue_video.size + cache_ptr->queue_sub.size;
    int secure_mem = cache_ptr->max_cache_mem - 3145728; //3M left
    if (avpkt_total_mem >= secure_mem) {
        if ((cache_ptr->has_audio && cache_ptr->queue_audio.nb_packets < cache_ptr->queue_audio.max_packets)
            || (cache_ptr->has_video && cache_ptr->queue_video.nb_packets < cache_ptr->queue_video.max_packets)) {
            cache_ptr->audio_max_packet = cache_ptr->queue_audio.nb_packets;
            cache_ptr->video_max_packet = cache_ptr->queue_video.nb_packets;
            cache_ptr->queue_audio.max_packets = cache_ptr->queue_audio.nb_packets;
            cache_ptr->queue_video.max_packets = cache_ptr->queue_video.nb_packets;

            cache_ptr->queue_audio.frames_max_seekbackword = (int)(cache_ptr->queue_audio.frames_backward_level * cache_ptr->queue_audio.max_packets);
            if (cache_ptr->queue_audio.frames_max_seekbackword == 0) {
                cache_ptr->queue_audio.frames_max_seekbackword = 1;
            }
            cache_ptr->queue_audio.frames_max_seekforword = (cache_ptr->queue_audio.max_packets - cache_ptr->queue_audio.frames_max_seekbackword);

            cache_ptr->queue_video.frames_max_seekbackword = (int)(cache_ptr->queue_video.frames_backward_level * cache_ptr->queue_video.max_packets);
            if (cache_ptr->queue_video.frames_max_seekbackword == 0) {
                cache_ptr->queue_video.frames_max_seekbackword = 1;
            }
            cache_ptr->queue_video.frames_max_seekforword = (cache_ptr->queue_video.max_packets - cache_ptr->queue_video.frames_max_seekbackword);
            if (cache_ptr->keepframes >= cache_ptr->queue_video.max_packets / 10) {
                cache_ptr->keepframes = cache_ptr->queue_video.max_packets / 10;
                if (cache_ptr->keepframes <= 50) {
                    cache_ptr->keepframes = 50;
                }
            }
            log_print("%s cache use mem reach %dB, high, should not malloc again\n", __FUNCTION__, avpkt_total_mem);
            log_print("%s modify max_packets, a_max:%d, a_canread:%d, v_max:%d, v_canread:%d,keepframes:%d\n", __FUNCTION__,
                      cache_ptr->queue_audio.max_packets, cache_ptr->queue_audio.frames_for_seek_forward,
                      cache_ptr->queue_video.max_packets, cache_ptr->queue_video.frames_for_seek_forward, cache_ptr->keepframes);
            return 0;
        }
    }

    if (cache_ptr->queue_audio.nb_packets < cache_ptr->queue_audio.max_packets
        && cache_ptr->queue_video.nb_packets < cache_ptr->queue_video.max_packets) {
        return 1;
    }
    /*log_print("a_nb:%d, a_max:%d, a_back:%d, a_forward:%d, a_maxb:%d, v_nb:%d, v_max:%d, v_back:%d, v_forward:%d, v_maxb:%d",
        cache_ptr->queue_audio.nb_packets,
        cache_ptr->queue_audio.max_packets,
        cache_ptr->queue_audio.frames_for_seek_backward,
        cache_ptr->queue_audio.frames_for_seek_forward,
        cache_ptr->queue_audio.frames_max_seekbackword,
        cache_ptr->queue_video.nb_packets,
        cache_ptr->queue_video.max_packets,
        cache_ptr->queue_video.frames_for_seek_backward,
        cache_ptr->queue_video.frames_for_seek_forward,
        cache_ptr->queue_video.frames_max_seekbackword);*/

    if (cache_ptr->has_audio) {
        acanput = avpkt_cache_queue_check_can_put(&cache_ptr->queue_audio, current_ms);
    }

    if (cache_ptr->has_video) {
        vcanput = avpkt_cache_queue_check_can_put(&cache_ptr->queue_video, current_ms);
    }

    return (acanput && vcanput);
}

/*
small_flag: 1-use small, 0-use big, -1-use the small abs(diff)
*/
static int avpkt_cache_queue_seek_bypts(PacketQueue *q, int64_t pts, int small_flag)
{
    /*1.seek by pts*/
    int ret = -1;
    int64_t queueHeadPktPts = -1;
    int64_t queueTailPktPts = -1;
    int64_t queueCurPktPts = -1;
    int64_t seekPts = -1;
    int64_t diff_pts_1 = -1;
    int64_t diff_pts_2 = -1;
    MyAVPacketList *mypktl = NULL;
    MyAVPacketList *mypktl_1 = NULL;
    MyAVPacketList *mypktl_2 = NULL;
    MyAVPacketList *mypktl_cur = NULL;
    int seek_sucess = 0;

    if (q == NULL || q->first_pkt == NULL || q->last_pkt == NULL || q->cur_pkt == NULL) {
        return -1;
    }

    cache_lock(&q->lock);
    queueHeadPktPts = q->head_valid_pts;
    queueTailPktPts = q->tail_valid_pts;
    //queueCurPktPts = q->cur_valid_pts;
    mypktl_cur = q->cur_pkt;
    if (q->cur_pkt->pts == AV_NOPTS_VALUE) {
        //find near non-pts pkt
        log_print("curpts:0x%llx, cur:0x%p, last:0x%p", mypktl_cur->pts, mypktl_cur, q->last_pkt);
        if (mypktl_cur->frame_id == q->last_pkt->frame_id) {
            //search backward
            //mypktl = mypktl_cur->priv;
            mypktl = mypktl_cur;
            for (; mypktl != NULL && mypktl->pts == AV_NOPTS_VALUE; mypktl = mypktl->priv) {
            }

            if (mypktl != NULL) {
                q->cur_pkt = mypktl;
                q->cur_valid_pts = q->cur_pkt->pts;
                mypktl_1 = mypktl;
                for (; mypktl_1 != NULL && mypktl_1->frame_id <= mypktl_cur->frame_id; mypktl_1 = mypktl_1->next) {
                    if (mypktl_1->used == 1) {
                        mypktl_1->used = 0;
                        q->frames_out--;
                        q->frames_for_seek_forward++;
                        q->frames_for_seek_backward--;
                    }
                }

                log_print("search backward:curpts:0x%llx", q->cur_valid_pts);
            }
        } else {
            //search forward
            //mypktl = mypktl_cur->next;
            mypktl = mypktl_cur;
            for (; mypktl != NULL && mypktl->pts == AV_NOPTS_VALUE; mypktl = mypktl->next) {
            }

            if (mypktl != NULL) {
                q->cur_pkt = mypktl;
                q->cur_valid_pts = q->cur_pkt->pts;
                mypktl_1 = mypktl_cur;
                for (; mypktl_1 != NULL && mypktl_1->frame_id < q->cur_pkt->frame_id; mypktl_1 = mypktl_1->next) {
                    if (mypktl_1->used == 0) {
                        mypktl_1->used = 1;
                        q->frames_out++;
                        q->frames_for_seek_forward--;
                        q->frames_for_seek_backward++;
                    }
                }

                if (mypktl_1->frame_id == q->cur_pkt->frame_id) {
                    if (mypktl_1->used == 1) {
                        mypktl_1->used = 0;
                        q->frames_out--;
                        q->frames_for_seek_forward++;
                        q->frames_for_seek_backward--;
                    }
                }

                log_print("search forward:curpts:0x%llx", q->cur_pkt->pkt.pts);
            }
        }
    }

    mypktl_cur = q->cur_pkt;

    q->cur_valid_pts = mypktl_cur->pts;
    queueCurPktPts = q->cur_valid_pts;
    //seek use right pts,should conver by timebase
    if (q->timebase) {
        seekPts = (double)pts / q->timebase;
    } else {
        seekPts = pts;
    }

    log_print("seekbypts-firstpts:0x%llx, headPts:0x%llx, curPts:0x%llx, tailPts:0x%llx, seekPts:0x%llx,timebase:%f",
              q->first_valid_pts, queueHeadPktPts, queueCurPktPts, queueTailPktPts, seekPts, q->timebase);

    q->dropref_pts = -1;
    mypktl = NULL;
    mypktl_1 = NULL;
    mypktl_2 = NULL;
    if (seekPts < queueHeadPktPts) {
        log_print("%s out of cache (small than first)", __FUNCTION__);
    } else if (seekPts <= queueCurPktPts) {
        //changed to whole queue,because maybe not searched when keyframe pts in<=queueCurPktPts
        mypktl = q->first_pkt;
        mypktl_cur = q->cur_pkt;
        log_print("first_pkt.frame_id:%lld, cur_pkt.frame_id:%lld, last_pkt.frame_id:%lld, searchbykeyframe:%d\\n",
                  mypktl->frame_id, mypktl_cur->frame_id, q->last_pkt->frame_id, s_avpkt_cache.search_by_keyframe);
        for (; mypktl != NULL && mypktl->frame_id <= q->last_pkt->frame_id; mypktl = mypktl->next) {
            //q->cur_pkt = mypktl;
            //log_print("mypktl.frame_id:%lld, pts:%llx,dts:%lld,mypktl->pts:%llx, key:%d\n",
            //mypktl->frame_id, mypktl->pkt.pts,mypktl->pkt.dts,mypktl->pts, ((mypktl->pkt.flags & AV_PKT_FLAG_KEY)));
            if (mypktl->pts != AV_NOPTS_VALUE && (s_avpkt_cache.search_by_keyframe == 0 || (mypktl->pkt.flags & AV_PKT_FLAG_KEY))) {
                if (mypktl_1 == NULL) {
                    mypktl_1 = mypktl;
                    mypktl_2 = mypktl;
                } else {
                    mypktl_1 = mypktl_2;
                    mypktl_2 = mypktl;
                }
                if (mypktl_2->pts  >= seekPts) {
                    seek_sucess = 1;
                    break;
                }
            }
            if (mypktl->frame_id == q->last_pkt->frame_id) {
                break;
            }
        }

        if (!seek_sucess) {
            mypktl = q->first_pkt;
            mypktl_1 = NULL;
        }
        for (; !seek_sucess && mypktl != NULL && mypktl->frame_id <= q->last_pkt->frame_id; mypktl = mypktl->next) {
            //q->cur_pkt = mypktl;
            if (mypktl->pts != AV_NOPTS_VALUE) {
                if (mypktl_1 == NULL) {
                    mypktl_1 = mypktl;
                    mypktl_2 = mypktl;
                } else {
                    mypktl_1 = mypktl_2;
                    mypktl_2 = mypktl;
                }

                if (mypktl_2->pts  >= seekPts) {
                    log_print(" seek by keyframe fail,use no key frame");
                    seek_sucess = 1;
                    break;
                }
            }
            if (mypktl->frame_id == q->last_pkt->frame_id) {
                break;
            }
        }
        if (seek_sucess) {
            if (mypktl_2->pts == seekPts) {
                q->cur_pkt = mypktl_2;
            } else {
                if (small_flag == 1) {
                    //use small pts pkt
                    q->cur_pkt = mypktl_1;
                } else if (small_flag == 0) {
                    //use the big one
                    q->cur_pkt = mypktl_2;
                } else if (small_flag == -1) {
                    //use the small diff pts pkt
                    diff_pts_1 = mypktl_1->pts - seekPts;
                    diff_pts_2 = mypktl_2->pts - seekPts;
                    if (abs(diff_pts_1) <= abs(diff_pts_2)) {
                        q->cur_pkt = mypktl_1;
                    } else {
                        q->cur_pkt = mypktl_2;
                    }
                }
            }

            if (q->cur_pkt->pts  <= queueCurPktPts) {
                mypktl_2 = q->cur_pkt;
                for (; mypktl_2 != NULL && mypktl_2->frame_id <= mypktl_cur->frame_id; mypktl_2 = mypktl_2->next) {
                    if (mypktl_2->used == 1) {
                        mypktl_2->used = 0;
                        q->frames_out--;
                        q->frames_for_seek_forward++;
                        q->frames_for_seek_backward--;
                    }
                }
            } else {
                mypktl_2 = mypktl_cur;
                mypktl_cur = q->cur_pkt;
                for (; mypktl_2 != NULL && mypktl_2->frame_id < mypktl_cur->frame_id ; mypktl_2 = mypktl_2->next) {
                    if (mypktl_2->used == 0) {
                        mypktl_2->used = 1;
                        q->frames_out++;
                        q->frames_for_seek_forward--;
                        q->frames_for_seek_backward++;
                    }
                }

                if (mypktl_2 != NULL && mypktl_2->frame_id == mypktl_cur->frame_id) {
                    if (mypktl_2->used == 1) {
                        mypktl_2->used = 0;
                        q->frames_out--;
                        q->frames_for_seek_forward++;
                        q->frames_for_seek_backward--;
                    }
                }
            }

            q->cur_valid_pts = q->cur_pkt->pts;
            q->dropref_pts = q->cur_valid_pts;
            log_print("%d--seek to pkt->pts:0x%llx", q->stream_index, q->cur_pkt->pts);
            ret = 0;
        }
    } else if (seekPts <= queueTailPktPts) {
        mypktl_cur = q->cur_pkt;
        mypktl = q->cur_pkt;
        log_print("1-first_pkt.frame_id:%lld, cur_pkt.frame_id:%lld, last_pkt.frame_id:%lld, searchbykeyframe:%d\n",
                  mypktl->frame_id, mypktl_cur->frame_id, q->last_pkt->frame_id, s_avpkt_cache.search_by_keyframe);

        for (; mypktl != NULL && mypktl->frame_id <= q->last_pkt->frame_id; mypktl = mypktl->next) {
            //q->cur_pkt = mypktl;
            //log_print("mypktl.frame_id:%lld, pts:%llx, dts:%llx,mypktl->pts:%llx, key:%d\n",
            //  mypktl->frame_id, mypktl->pkt.pts, mypktl->pkt.dts,mypktl->pts,((mypktl->pkt.flags & AV_PKT_FLAG_KEY)));

            if (mypktl->pkt.pts != AV_NOPTS_VALUE
                && (s_avpkt_cache.search_by_keyframe == 0 || (mypktl->pkt.flags & AV_PKT_FLAG_KEY))) {
                if (mypktl_1 == NULL) {
                    mypktl_1 = mypktl;
                    mypktl_2 = mypktl;
                } else {
                    mypktl_1 = mypktl_2;
                    mypktl_2 = mypktl;
                }
                if (mypktl_2->pkt.pts >= seekPts) {
                    seek_sucess = 1;
                    break;
                }
            }
            if (mypktl->frame_id == q->last_pkt->frame_id) {
                break;
            }

        }

        if (!seek_sucess) {
            mypktl = q->first_pkt;
            mypktl_1 = NULL;
        }
        for (; !seek_sucess && mypktl != NULL && mypktl->frame_id <= q->last_pkt->frame_id; mypktl = mypktl->next) {
            //q->cur_pkt = mypktl;
            //log_print("mypktl.frame_id:%lld, pts:%llx, dts:%llx,mypktl->pts:%llx, key:%d\n",
            //  mypktl->frame_id, mypktl->pkt.pts, mypktl->pkt.dts,mypktl->pts,((mypktl->pkt.flags & AV_PKT_FLAG_KEY));

            if (mypktl->pts != AV_NOPTS_VALUE) {
                if (mypktl_1 == NULL) {
                    mypktl_1 = mypktl;
                    mypktl_2 = mypktl;
                } else {
                    mypktl_1 = mypktl_2;
                    mypktl_2 = mypktl;
                }

                if (mypktl_2->pts >= seekPts) {
                    log_print(" seek by keyframe fail,use no key frame");
                    seek_sucess = 1;
                    break;
                }

            }

            if (mypktl->frame_id == q->last_pkt->frame_id) {
                break;
            }
        }
        if (seek_sucess) {
            if (mypktl_2->pts == seekPts) {
                q->cur_pkt = mypktl_2;
            } else {
                if (small_flag == 1) {
                    //use small pts pkt
                    q->cur_pkt = mypktl_1;
                } else if (small_flag == 0) {
                    //use the big one
                    q->cur_pkt = mypktl_2;
                } else if (small_flag == -1) {
                    //use the small diff pts pkt
                    diff_pts_1 = mypktl_1->pts - seekPts;
                    diff_pts_2 = mypktl_2->pts - seekPts;
                    if (abs(diff_pts_1) <= abs(diff_pts_2)) {
                        q->cur_pkt = mypktl_1;
                    } else {
                        q->cur_pkt = mypktl_2;
                    }
                }
            }

            mypktl_2 = mypktl_cur;
            for (; mypktl_2 != NULL && mypktl_2->frame_id < q->cur_pkt->frame_id ; mypktl_2 = mypktl_2->next) {
                if (mypktl_2->used == 0) {
                    mypktl_2->used = 1;
                    q->frames_for_seek_backward++;
                    q->frames_for_seek_forward--;
                    q->frames_out++;
                }
            }

            if (mypktl_2 != NULL && mypktl_2->frame_id == q->cur_pkt->frame_id) {
                if (mypktl_2->used == 1) {
                    mypktl_2->used = 0;
                    q->frames_for_seek_forward++;
                    q->frames_for_seek_backward--;
                    q->frames_out--;
                }
            }

            q->cur_valid_pts = q->cur_pkt->pts;
            q->dropref_pts = q->cur_valid_pts;
            log_print("%d--seek to pkt->pts:0x%llx", q->stream_index, q->cur_pkt->pts);
            ret = 0;
        }
    } else {
        log_print("%s out of cache (bigger than last)", __FUNCTION__);
    }

    if (mypktl != NULL && q->dropref_pts == -1) {
        log_print("mypktl->frame_id:%lld, last_pkt_frame_id:%lld\n",
                  mypktl->frame_id, q->last_pkt->frame_id);
    }

    cache_unlock(&q->lock);
    return ret;
}

static int avpkt_cache_queue_seektoend(PacketQueue *q)
{
    if (q->first_pkt == NULL || q->last_pkt == NULL || q->cur_pkt == NULL) {
        return -1;
    }

    cache_lock(&q->lock);
    MyAVPacketList *mypktl = NULL;
    mypktl = q->cur_pkt;
    for (; mypktl != NULL && mypktl->frame_id <= q->last_pkt->frame_id; mypktl = mypktl->next) {
        if (mypktl->used == 0) {
            mypktl->used = 1;
            q->frames_for_seek_backward++;
            q->frames_for_seek_forward--;
            q->frames_out++;
        }

        if (mypktl->pkt.pts != AV_NOPTS_VALUE) {
            q->cur_valid_pts = mypktl->pkt.pts;
        }
    }

    q->dropref_pts = q->cur_valid_pts;
    q->cur_pkt = q->last_pkt;
    cache_unlock(&q->lock);
    return 0;
}

static int avpkt_cache_queue_seek(PacketQueue *q, int64_t seekTimeMs)
{
    /*1.seek by pts*/
    int ret = -1;
    int64_t queueHeadPktPts = -1;
    int64_t queueTailPktPts = -1;
    int64_t queueCurPktPts = -1;
    int64_t seekPts = -1;
    int64_t diff_pts_1 = -1;
    int64_t diff_pts_2 = -1;
    MyAVPacketList *mypktl = NULL;
    MyAVPacketList *mypktl_1 = NULL;
    MyAVPacketList *mypktl_2 = NULL;
    MyAVPacketList *mypktl_cur = NULL;

    if (q->first_pkt == NULL || q->last_pkt == NULL || q->cur_pkt == NULL) {
        return -1;
    }

    seekPts = seekTimeMs * 90;

    if (q->stream_index == s_avpkt_cache.video_index) {
        ret = avpkt_cache_queue_seek_bypts(q, seekPts, 0);//use big one
    } else if (q->stream_index == s_avpkt_cache.audio_index) {
        ret = avpkt_cache_queue_seek_bypts(q, seekPts, 1);//use small one
    }

    return ret;
}

/*
call this function when can seektime be searched in both audio and video
*/
static int avpkt_cache_seek(av_packet_cache_t *cache_ptr, int64_t seekTimeSec)
{
    int ret = 0;
    /*trust keyframe pts*/
    if (cache_ptr->has_video) {
        log_print("%s: video seek start\n", __FUNCTION__);
        ret = avpkt_cache_queue_seek(&cache_ptr->queue_video, seekTimeSec);
        if (ret == -1) {
            log_print("video seek fails");
            return ret;
        } else {
            log_print("video seek succeeds, cread:%d, headPts:0x%llx, tailPts:0x%llx, seekPktPts:0x%llx",
                      cache_ptr->queue_video.frames_for_seek_forward, cache_ptr->queue_video.head_valid_pts,
                      cache_ptr->queue_video.tail_valid_pts, cache_ptr->queue_video.cur_valid_pts);
        }
    }

    if (cache_ptr->has_audio) {
        if (cache_ptr->queue_video.dropref_pts != -1
            && cache_ptr->queue_video.dropref_pts != AV_NOPTS_VALUE) {
            int64_t dropref_pts;
            if (cache_ptr->queue_video.timebase) {
                dropref_pts = cache_ptr->queue_video.dropref_pts * cache_ptr->queue_video.timebase;
            } else {
                dropref_pts = cache_ptr->queue_video.dropref_pts;
            }
            log_print("%s: audio seek start, refvpts:%lld,dropref_pts:%lld\n", __FUNCTION__, dropref_pts, cache_ptr->queue_video.dropref_pts);
            ret = avpkt_cache_queue_seek_bypts(&cache_ptr->queue_audio, dropref_pts, 1);//use small one
        } else {
            log_print("%s: audio seek start, pts:lld\n", __FUNCTION__, seekTimeSec);
            ret = avpkt_cache_queue_seek(&cache_ptr->queue_audio, seekTimeSec);
        }

        if (ret == -1) {
            log_print("audio seek fails");
            if (cache_ptr->trickmode == 1) {
                ret = 0;
                //seek to the end pkt
                log_print("status:ff/fb seek to the end pkt");
                avpkt_cache_queue_seektoend(&cache_ptr->queue_audio);
            }
        } else {
            log_print("audio seek succeeds, cread:%d, headPts:0x%llx, tailPts:0x%llx, seekPktPts:0x%llx",
                      cache_ptr->queue_audio.frames_for_seek_forward, cache_ptr->queue_audio.head_valid_pts,
                      cache_ptr->queue_audio.tail_valid_pts, cache_ptr->queue_audio.cur_valid_pts);
        }
    }

    return ret;
}

static int avpkt_cache_reset(av_packet_cache_t *cache_ptr)
{
    play_para_t *player = (play_para_t *)cache_ptr->context;
    int64_t newstarttime_ms = 0;
    int retry = 0;

    if (cache_ptr->reading == 1) {
        avpkt_cache_interrupt_read(cache_ptr);
        while (retry < 200) {
            if (cache_ptr->reading == 0) {
                break;
            }
            retry++;
            amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
        }
        avpkt_cache_uninterrupt_read(cache_ptr);
        //cache_ptr->error = 0;
    }

    if (cache_ptr->has_audio) {
        log_print("%s release queue audio\n", __FUNCTION__);
        packet_queue_flush(&cache_ptr->queue_audio);
        cache_ptr->audio_count = cache_ptr->queue_audio.nb_packets;
        cache_ptr->audio_size = cache_ptr->queue_audio.size;
        cache_ptr->audio_cachems = cache_ptr->queue_audio.bak_cache_pts;
    }
    if (cache_ptr->has_video) {
        log_print("%s release queue video\n", __FUNCTION__);
        packet_queue_flush(&cache_ptr->queue_video);
        cache_ptr->video_count = cache_ptr->queue_video.nb_packets;
        cache_ptr->video_size = cache_ptr->queue_video.size;
        cache_ptr->video_cachems = cache_ptr->queue_video.bak_cache_pts;
    }
    if (cache_ptr->has_sub) {
        packet_queue_flush(&cache_ptr->queue_sub);
        cache_ptr->sub_count = cache_ptr->queue_sub.nb_packets;
        cache_ptr->sub_size = cache_ptr->queue_sub.size;
        cache_ptr->sub_cachems = cache_ptr->queue_sub.bak_cache_pts;
    }

    if (player->playctrl_info.time_point < 0) {
        newstarttime_ms = 0;
    } else {
        newstarttime_ms = (int64_t)(player->playctrl_info.time_point * 1000);
    }

    cache_ptr->seekTimeMs = newstarttime_ms;
    cache_ptr->starttime_ms = newstarttime_ms;
    cache_ptr->discontinue_current_ms = 0;
    cache_ptr->discontinue_current_ms_flag = 0;
    cache_ptr->discontinue_current_ms_checked = 0;
    cache_ptr->first_apts = -1;
    cache_ptr->first_vpts = -1;
    cache_ptr->first_spts = -1;
    cache_ptr->discontinue_current_ms_checked = 0;
    cache_ptr->error = 0;
    cache_ptr->netdown = 0;
    cache_ptr->last_netdown_state = 0;

    if (cache_ptr->enable_keepframes == 1) {
        cache_ptr->resetkeepframes = 0;
        cache_ptr->startenterkeepframes = 0;
    }

    cache_ptr->start_avsync_finish = 0;

    return 0;
}

static int avpkt_cache_release(av_packet_cache_t *cache_ptr)
{
    log_print("release total mem:%d",
              (cache_ptr->queue_video.size + cache_ptr->queue_audio.size + cache_ptr->queue_sub.size));
    if (cache_ptr->has_audio) {
        log_print("%s release queue audio, mem size:%d\n", __FUNCTION__, cache_ptr->queue_audio.size);
        packet_queue_destroy(&cache_ptr->queue_audio);
        cache_ptr->audio_count = cache_ptr->queue_audio.nb_packets;
        cache_ptr->audio_size = cache_ptr->queue_audio.size;
        cache_ptr->audio_cachems = cache_ptr->queue_audio.bak_cache_pts;
    }
    if (cache_ptr->has_video) {
        log_print("%s release queue video, mem size:%d\n", __FUNCTION__, cache_ptr->queue_video.size);
        packet_queue_destroy(&cache_ptr->queue_video);
        cache_ptr->video_count = cache_ptr->queue_video.nb_packets;
        cache_ptr->video_size = cache_ptr->queue_video.size;
        cache_ptr->video_cachems = cache_ptr->queue_video.bak_cache_pts;
    }
    if (cache_ptr->has_sub) {
        packet_queue_destroy(&cache_ptr->queue_sub);
        cache_ptr->sub_count = cache_ptr->queue_sub.nb_packets;
        cache_ptr->sub_size = cache_ptr->queue_sub.size;
        cache_ptr->sub_cachems = cache_ptr->queue_sub.bak_cache_pts;
    }

    cache_ptr->first_apts = -1;
    cache_ptr->first_vpts = -1;
    cache_ptr->first_spts = -1;
    cache_ptr->discontinue_current_ms_checked = 0;
    cache_ptr->context = NULL;
    cache_ptr->error = 0;
    cache_ptr->read_frames = 0;
    cache_ptr->max_cache_mem = 0;

    cache_ptr->start_avsync_finish = 0;

    if (cache_ptr->enable_keepframes == 1) {
        cache_ptr->resetkeepframes = 0;
        cache_ptr->keepframes = am_getconfig_int_def("libplayer.cache.keepframes", 125);
        cache_ptr->enterkeepframes = am_getconfig_int_def("libplayer.cache.enterkeepframes", 300);
        cache_ptr->startenterkeepframes = 0;
    }

    memset(cache_ptr, 0, sizeof(*cache_ptr));
    return 0;
}

int avpkt_cache_checkvlevel(av_packet_cache_t *cache_ptr, float level)
{
    play_para_t *player = (play_para_t *)s_avpkt_cache.context;

    if (player->vstream_info.has_video && player->state.video_bufferlevel >= level) {
        return 1;
    }

    return 0;
}

static int packet_update_bufed_time(void)
{
    int frame_dur_pts = 0;
    int64_t total_cache_ms = 0;
    int64_t cache_ms_bypts = 0;
    int64_t bak_calc_pts = 0;
    int64_t discontinue_pts = 0;
    int bufed_time = 0;

    frame_dur_pts = s_avpkt_cache.queue_video.frame_dur_pts;
    total_cache_ms = (int64_t)((s_avpkt_cache.queue_video.frames_in * frame_dur_pts) / 90);
    bak_calc_pts = s_avpkt_cache.queue_video.bak_cache_pts;
    discontinue_pts = s_avpkt_cache.queue_video.discontinue_pts;

    if (s_avpkt_cache.queue_video.discontinue_flag == 1) {
        bak_calc_pts -= discontinue_pts;
    }

    if (bak_calc_pts > 0) {
        cache_ms_bypts = (int64_t)(bak_calc_pts / 90);
        total_cache_ms = cache_ms_bypts;
    }

    bufed_time = (int)(total_cache_ms / 1000);

    return bufed_time;
}

static int64_t packet_calc_cachetime_by_player_current_ms(PacketQueue *q, int64_t player_current_ms, int stream_idx, int debug)
{
    int64_t cache_ms = 0;
    int64_t total_cache_ms = 0;
    int64_t playtime_ms = 0;
    int64_t queue_left_ms = 0;
    int64_t left_frames = 0;
    int frame_dur_pts = 0;
    int64_t pts_last = q->tail_valid_pts;
    int64_t pts_first = q->first_keyframe_pts;
    int64_t diff_pts = -1;
    int64_t cache_ms_bypts = 0;

    play_para_t *player = (play_para_t *)s_avpkt_cache.context;
    frame_dur_pts = q->frame_dur_pts;
    left_frames = (q->frames_in - q->frames_out);

    if (debug) {
        log_print("\nnb_packets:%d, max_packets:%d, frame_dur:%d, in:%lld, out:%lld, left:%lld, backnum:%d, forwnum:%d\n",
                  q->nb_packets, q->max_packets, q->frame_dur_pts, q->frames_in, q->frames_out, left_frames,
                  q->frames_for_seek_backward, q->frames_for_seek_forward);
    }

    cache_lock(&q->lock);

    {
        if (frame_dur_pts <= 0) {
            //use default frame_dur
            frame_dur_pts = 3600;
        }

        total_cache_ms = (int64_t)((q->frames_in * frame_dur_pts) / 90);
        playtime_ms = (player_current_ms - (s_avpkt_cache.starttime_ms + s_avpkt_cache.discontinue_current_ms));
        cache_ms = total_cache_ms - playtime_ms;

        //if frame_dur normal value, refer pts
        if (pts_first != -1) {
            int64_t bak_calc_pts = q->bak_cache_pts;
            int64_t discontinue_pts = q->discontinue_pts;
            if (q->discontinue_flag == 1) {
                bak_calc_pts -= discontinue_pts;
            }
            cache_ms_bypts = ((int64_t)(bak_calc_pts / 90) - playtime_ms);
            if (debug) {
                log_print("bak_calc_pts:0x%llx, 0x%llx", bak_calc_pts, discontinue_pts);
                log_print("cache_ms_by_duration:%lld, cache_ms_by_pts:%lld", cache_ms, cache_ms_bypts);
            }
            cache_ms = cache_ms_bypts;
        }
        //end

        if (s_avpkt_cache.enable_keepframes == 1) {
            if (s_avpkt_cache.startenterkeepframes == 1 && left_frames >= s_avpkt_cache.keepframes) {
                cache_ms -= (int64_t)((s_avpkt_cache.keepframes * q->frame_dur_pts) / 90);
            }
        }

        //compare pts
        if (s_avpkt_cache.netdown == 1 && player->state.status == PLAYER_BUFFERING) {
            cache_ms = 0;
        }
        //end

        if (debug) {
            log_print("current_ms:%lld, starttime_ms:%lld, discont_ms:%lld",
                      player_current_ms, s_avpkt_cache.starttime_ms, s_avpkt_cache.discontinue_current_ms);
            log_print("duration:in:%lld, out:%lld, frame_dur_pts:%d, total_cache_ms:%lld, playtime_ms:%lld, cache_ms:%lld",
                      q->frames_in, q->frames_out, frame_dur_pts, total_cache_ms, playtime_ms, cache_ms);
        }
    }

    if (cache_ms < 0) {
        cache_ms = 0;
    }
    cache_unlock(&q->lock);

    return cache_ms;
}

/*
    refer in player current_ms
*/
int64_t avpkt_cache_getcache_time_by_streamindex(play_para_t *player, int stream_idx)
{
    int ret = 0;
    int64_t cache_ms = 0;
    av_packet_cache_t *cache_ptr = &s_avpkt_cache;

    if (cache_ptr->state != 2) {
        log_print("cache state:%d\n", cache_ptr->state);
        return 0;
    }

    if (cache_ptr->discontinue_current_ms_checked == 1
        && cache_ptr->queue_audio.bak_cache_pts <= 0
        && cache_ptr->queue_video.bak_cache_pts <= 0) {
        return 0;
    }

    int64_t current_ms = (int64_t)player->state.current_ms;
    int64_t diff_ms = 0;
    if (s_avpkt_cache.discontinue_current_ms_checked == 1) {
        s_avpkt_cache.currenttime_ms = current_ms;
        diff_ms = s_avpkt_cache.currenttime_ms - s_avpkt_cache.last_currenttime_ms;
        if (abs(diff_ms) >= CURRENT_TIME_MS_DISCONTINUE) {
            s_avpkt_cache.discontinue_current_ms += diff_ms;
            log_print("current time discontinue: %lld - > %lld", s_avpkt_cache.last_currenttime_ms, current_ms);
        }
        s_avpkt_cache.last_currenttime_ms = current_ms;
    }

    if (stream_idx == cache_ptr->audio_index) {
        if (cache_ptr->audio_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_audio, current_ms, stream_idx, 1);
        }
        log_print("audio cache_ms:%lld\n", cache_ms);
    } else if (stream_idx == cache_ptr->video_index) {
        if (cache_ptr->video_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_video, current_ms, stream_idx, 1);
        }
        log_print("video cache_ms:%lld\n", cache_ms);
    } else if (stream_idx == cache_ptr->sub_index) {
        if (cache_ptr->video_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_sub, current_ms, stream_idx, 1);
        }
        log_print("sub cache_ms:%lld\n", cache_ms);
    } else {

    }

    return cache_ms;
}

/*
    refer in player current_ms; the same as avpkt_cache_getcache_time_by_streamindex
    for player buffering mechanism
*/
int64_t avpkt_cache_getcache_time(play_para_t *player, int stream_idx)
{
    int ret = 0;
    int64_t cache_ms = 0;
    av_packet_cache_t *cache_ptr = &s_avpkt_cache;

    if (cache_ptr->state != 2) {
        //log_print("cache state:%d\n", cache_ptr->state);
        return 0;
    }

    if (cache_ptr->discontinue_current_ms_checked == 1
        && cache_ptr->queue_audio.bak_cache_pts <= 0
        && cache_ptr->queue_video.bak_cache_pts <= 0) {
        return 0;
    }

    int64_t current_ms = (int64_t)player->state.current_ms;
    int64_t diff_ms = 0;
    if (s_avpkt_cache.discontinue_current_ms_checked == 1) {
        s_avpkt_cache.currenttime_ms = current_ms;
        diff_ms = s_avpkt_cache.currenttime_ms - s_avpkt_cache.last_currenttime_ms;
        if (abs(diff_ms) >= CURRENT_TIME_MS_DISCONTINUE) {
            s_avpkt_cache.discontinue_current_ms += diff_ms;
            //log_print("current time discontinue: %lld - > %lld", s_avpkt_cache.last_currenttime_ms, current_ms);
        }
        s_avpkt_cache.last_currenttime_ms = current_ms;
    }

    if (stream_idx == cache_ptr->audio_index) {
        if (cache_ptr->audio_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_audio, current_ms, stream_idx, 0);
        }
        //log_print("audio cache_ms:%lld\n", cache_ms);
    } else if (stream_idx == cache_ptr->video_index) {
        if (cache_ptr->video_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_video, current_ms, stream_idx, 0);
        }
        //log_print("video cache_ms:%lld\n", cache_ms);
    } else if (stream_idx == cache_ptr->sub_index) {
        if (cache_ptr->video_count > 0) {
            cache_ms = packet_calc_cachetime_by_player_current_ms(&cache_ptr->queue_sub, current_ms, stream_idx, 0);
        }
        //log_print("sub cache_ms:%lld\n", cache_ms);
    } else {

    }

    return cache_ms;
}


//recv stop,seek,start,pause,resume
int avpkt_cache_set_cmd(AVPacket_Cache_E cmd)
{
    int retry = 10;
    while (retry > 0) {
        if (s_avpkt_cache.state == 0) {
            log_print("%s:%d not inited, return\n", __FUNCTION__, __LINE__);
            amthreadpool_thread_usleep(10 * 1000);
            retry--;
        } else {
            break;
        }
    }

    if (retry == 0) {
        return 0;
    }
    play_para_t *player = (play_para_t *)s_avpkt_cache.context;
    s_avpkt_cache.cmd = cmd;
    if (s_avpkt_cache.cmd == CACHE_CMD_STOP) {
        s_avpkt_cache.state = 0;
    } else if (cmd == CACHE_CMD_START) {
        s_avpkt_cache.state = 2;
    } else if (cmd == CACHE_CMD_SEARCH_START) {
        s_avpkt_cache.state = 1;
        if (s_avpkt_cache.trickmode == 1 && player->playctrl_info.f_step == 0) {
            s_avpkt_cache.trickmode = 0;
        }
        s_avpkt_cache.discontinue_current_ms_checked = 0;
    } else if (cmd == CACHE_CMD_SEARCH_OK) {
        s_avpkt_cache.trickmode = 0;
        s_avpkt_cache.need_avsync = 1;
        s_avpkt_cache.fffb_start = 0;
        s_avpkt_cache.fffb_out_frames = am_getconfig_int_def("libplayer.cache.fffbframes", 61);
        s_avpkt_cache.state = 2;
    } else if (cmd == CACHE_CMD_SEEK_OUT_OF_CACHE) {
        s_avpkt_cache.state = 1;
    } else if (cmd == CACHE_CMD_SEEK_IN_CACHE) {
        s_avpkt_cache.state = 1;
    } else if (cmd == CACHE_CMD_RESET) {
        s_avpkt_cache.state = 1;
        avpkt_cache_reset(&s_avpkt_cache);
    } else if (cmd == CACHE_CMD_RESET_OK) {
        s_avpkt_cache.trickmode = 0;
        s_avpkt_cache.fffb_start = 0;
        s_avpkt_cache.fffb_out_frames = am_getconfig_int_def("libplayer.cache.fffbframes", 61);
        s_avpkt_cache.state = 2;
    } else if (cmd == CACHE_CMD_FFFB) {
        s_avpkt_cache.state = 1;
        s_avpkt_cache.trickmode = 1;
        s_avpkt_cache.fffb_start = 0;
        s_avpkt_cache.discontinue_current_ms_checked = 0;
        s_avpkt_cache.fffb_out_frames = am_getconfig_int_def("libplayer.cache.fffbframes", 61);
    } else if (cmd == CACHE_CMD_FFFB_OK) {
        s_avpkt_cache.need_avsync = 0;
        s_avpkt_cache.fffb_start = 1;
        s_avpkt_cache.fffb_out_frames = am_getconfig_int_def("libplayer.cache.fffbframes", 61);
        s_avpkt_cache.state = 2;
    }

    log_print("%s cmd:%d, state:%d, vin:%lld, out:%lld, forward:%d, backward:%d, fbfr:%d, fstart:%d, tm:%d\n",
              __FUNCTION__, s_avpkt_cache.cmd, s_avpkt_cache.state,
              s_avpkt_cache.queue_video.frames_in, s_avpkt_cache.queue_video.frames_out,
              s_avpkt_cache.queue_video.frames_for_seek_forward,
              s_avpkt_cache.queue_video.frames_for_seek_backward,
              s_avpkt_cache.fffb_out_frames,
              s_avpkt_cache.fffb_start,
              s_avpkt_cache.trickmode);

    return 0;
}

static int64_t avpkt_cache_queue_search_bypts(PacketQueue *q, int64_t pts)
{
    if (q == NULL) {
        return -1;
    }

    if (pts <= q->tail_valid_pts && pts >= q->head_valid_pts) {
        return 0;
    } else {
        return -1;
    }
}

static int64_t avpkt_cache_queue_search(PacketQueue *q, int64_t seekTimeMs)
{
    /*method1: trust pts*/
    int64_t queueHeadPktPts = -1;
    int64_t queueTailPktPts = -1;
    int64_t seekPts = -1;

    if (q->first_pkt == NULL || q->last_pkt == NULL || q->nb_packets <= 0) {
        return -1;
    }

    cache_lock(&q->lock);

    queueHeadPktPts = q->head_valid_pts;
    queueTailPktPts = q->tail_valid_pts;
    seekPts = seekTimeMs * 90;

    log_print("%s headPktPts:0x%llx, TailPktPts:0x%llx, seekPts:0x%llx",
              __FUNCTION__, queueHeadPktPts, queueTailPktPts, seekPts);

    if (seekPts <= queueTailPktPts && seekPts >= queueHeadPktPts) {
        //do nothing
    } else {
        seekPts = -1;
    }

    //calc cache time
    //calc can seek forward time_ms
    //end

    //cal can seek backward time_ms
    //end
    cache_unlock(&q->lock);

    return seekPts;
}

static int avpkt_cache_queue_dropframes(PacketQueue *q, int64_t dst_pts, int small_flag)
{
    MyAVPacketList *mypktl = NULL;
    MyAVPacketList *mypktl_1 = NULL;
    MyAVPacketList *mypktl_2 = NULL;
    MyAVPacketList *mypktl_cur = NULL;
    MyAVPacketList *mypktl_newcur = NULL;
    int64_t diff_pts1 = -1;
    int64_t diff_pts2 = -1;
    int get_vaild_pts = 0;
    int ret = 0;

    if (q->cur_pkt == NULL) {
        return -1;
    }

    cache_lock(&q->lock);

    mypktl_cur = q->cur_pkt;
    for (; mypktl_cur != NULL;) {
        if (mypktl_cur->pts != AV_NOPTS_VALUE) {
            get_vaild_pts = 1;
            break;
        }
        mypktl_cur = mypktl_cur->next;
    }
    if (!get_vaild_pts) {
        cache_unlock(&q->lock);
        return -1;
    }

    mypktl_cur = q->cur_pkt;
    for (; mypktl_cur != NULL && mypktl_cur->pkt.pts == AV_NOPTS_VALUE;) {
        // find valid pts cur
        if (mypktl_cur->used == 0) {
            mypktl_cur->used = 1;
        }

        q->frames_out++;
        q->frames_for_seek_forward--;
        q->frames_for_seek_backward++;
        mypktl_cur = mypktl_cur->next;
        q->cur_pkt = mypktl_cur;
    }
    mypktl = q->cur_pkt;
    q->cur_valid_pts = mypktl->pkt.pts;
    q->dropref_pts = q->cur_valid_pts;

    for (; mypktl != NULL; mypktl = mypktl->next) {
        if (mypktl->pkt.pts != AV_NOPTS_VALUE
            && (q->trust_keyframe == 0 || (mypktl->pkt.flags & AV_PKT_FLAG_KEY)
                || q->stream_index == s_avpkt_cache.audio_index)) {
            if (mypktl_1 == NULL) {
                mypktl_1 = mypktl;
                mypktl_2 = mypktl;
            } else {
                mypktl_1 = mypktl_2;
                mypktl_2 = mypktl;
            }

            if (mypktl_2->pkt.pts >= dst_pts) {
                if (mypktl_2->pkt.pts == dst_pts) {
                    mypktl_newcur = mypktl_2;
                } else {
                    if (small_flag == 1) {
                        mypktl_newcur = mypktl_1;
                    } else if (small_flag == 0) {
                        mypktl_newcur = mypktl_2;
                    } else if (small_flag == -1) {
                        diff_pts1 = mypktl_1->pkt.pts - dst_pts;
                        diff_pts2 = mypktl_2->pkt.pts - dst_pts;
                        if (abs(diff_pts1) <= abs(diff_pts2)) {
                            mypktl_newcur = mypktl_1;
                        } else {
                            mypktl_newcur = mypktl_2;
                        }
                    }
                }

                //log_print("new keyframe:0x%llx", mypktl->pkt.pts);
                break;
            }
        }
    }

    if (mypktl_newcur != NULL) {
        mypktl = mypktl_cur;
        for (; mypktl->frame_id < mypktl_newcur->frame_id; mypktl = mypktl->next) {
            if (mypktl->used == 0) {
                mypktl->used = 1;
            }
            q->frames_for_seek_backward++;
            q->frames_for_seek_forward--;
            q->frames_out++;
        }

        q->cur_pkt = mypktl_newcur;
        q->cur_valid_pts = mypktl_newcur->pkt.pts;
        q->dropref_pts = q->cur_valid_pts;
    } else {
        ret = -1;
    }

    if (q->cur_pkt->used == 1) {
        q->cur_pkt->used = 0;
        q->frames_for_seek_backward--;
        q->frames_for_seek_forward++;
        q->frames_out--;
    }

    cache_unlock(&q->lock);

    return ret;
}

static int avpkt_cache_interrupt_read(av_packet_cache_t *cache_ptr)
{
    if (cache_ptr == NULL) {
        return 0;
    }

    play_para_t *player = (play_para_t *)cache_ptr->context;

    player->playctrl_info.ignore_ffmpeg_errors = 1;
    player->playctrl_info.temp_interrupt_ffmpeg = 1;
    set_black_policy(0);
    ffmpeg_interrupt_light(player->thread_mgt.pthread_id);

    return 0;
}

static int avpkt_cache_uninterrupt_read(av_packet_cache_t * cache_ptr)
{
    if (cache_ptr == NULL) {
        return 0;
    }

    play_para_t *player = (play_para_t *)cache_ptr->context;
    if (player->playctrl_info.temp_interrupt_ffmpeg) {
        player->playctrl_info.temp_interrupt_ffmpeg = 0;
        log_print("ffmpeg_uninterrupt tmped by avpkt cache!\n");
        ffmpeg_uninterrupt_light(player->thread_mgt.pthread_id);
        player->playctrl_info.ignore_ffmpeg_errors = 0;
    }

    return 0;
}

int avpkt_cache_search(play_para_t *player, int64_t seekTimeSec)
{
    int ret = 0;
    int cache_kick = 0;
    int64_t aSeekPts = 0;
    int64_t vSeekPts = 0;
    int live_reset = 0;

    avpkt_cache_set_cmd(CACHE_CMD_SEARCH_START);
    if (player == NULL || seekTimeSec < 0) {
        log_print("[%s]invalid param \n", __FUNCTION__);
        return -1;
    }

    if (s_avpkt_cache.state == 0) {
        log_print("[%s]state 0 \n", __FUNCTION__);
        return -1;
    }

    if (player->start_param != NULL) {
        log_print("%s:is_livemode:%d, seekTimeSec:%lld\n", __FUNCTION__, player->start_param->is_livemode, seekTimeSec);
        if (player->start_param->is_livemode == 1 && seekTimeSec == 0) {
            live_reset = 1;
            log_print("%s:hls live play reset\n", __FUNCTION__);
        }
    }

    s_avpkt_cache.seekTimeMs = seekTimeSec * 1000;
    s_avpkt_cache.start_avsync_finish = 0;
    s_avpkt_cache.queue_audio.dropref_pts = -1;
    s_avpkt_cache.queue_video.dropref_pts = -1;

    if (s_avpkt_cache.enable_seek_in_cache == 0) {
        log_print("disable seek in cache ");
        avpkt_cache_set_cmd(CACHE_CMD_SEEK_OUT_OF_CACHE);
        avpkt_cache_reset(&s_avpkt_cache);
        s_avpkt_cache.error = 0;
        s_avpkt_cache.starttime_ms = s_avpkt_cache.seekTimeMs;
        s_avpkt_cache.currenttime_ms = s_avpkt_cache.starttime_ms;
        s_avpkt_cache.last_currenttime_ms = s_avpkt_cache.starttime_ms;
        s_avpkt_cache.discontinue_current_ms = 0;
        s_avpkt_cache.discontinue_current_ms_flag = 0;
        s_avpkt_cache.discontinue_current_ms_checked = 0;
        return -1;
    }

    if (live_reset == 0) {
        if (s_avpkt_cache.has_video) {
            if ((vSeekPts = avpkt_cache_queue_search(&s_avpkt_cache.queue_video, s_avpkt_cache.seekTimeMs)) == -1) {
                log_print("%s seek video out of cache, nb_packets:%d\n", __FUNCTION__, s_avpkt_cache.queue_video.nb_packets);
            }
        }

        /*if (s_avpkt_cache.has_audio) {
            if ((aSeekPts = avpkt_cache_queue_search(&s_avpkt_cache.queue_audio, s_avpkt_cache.seekTimeMs)) == -1) {
                log_print("%s seek audio out of cache\n", __FUNCTION__);
            }
        }*/

        if (vSeekPts != -1) {
            ret = avpkt_cache_seek(&s_avpkt_cache, s_avpkt_cache.seekTimeMs);
            if (ret == 0) {
                cache_kick = 1;
            }
        }
    }

    if (cache_kick == 0) {
        //need reset
        avpkt_cache_set_cmd(CACHE_CMD_SEEK_OUT_OF_CACHE);
        avpkt_cache_reset(&s_avpkt_cache);
        s_avpkt_cache.error = 0;
        s_avpkt_cache.starttime_ms = s_avpkt_cache.seekTimeMs;
        s_avpkt_cache.currenttime_ms = s_avpkt_cache.starttime_ms;
        s_avpkt_cache.last_currenttime_ms = s_avpkt_cache.starttime_ms;
        s_avpkt_cache.discontinue_current_ms = 0;
        s_avpkt_cache.discontinue_current_ms_flag = 0;
        s_avpkt_cache.discontinue_current_ms_checked = 0;
        ret = -1;
    } else {
        s_avpkt_cache.start_avsync_finish = 1;
        avpkt_cache_set_cmd(CACHE_CMD_SEEK_IN_CACHE);
    }

    s_avpkt_cache.state = 1;

    return ret;
}

/*
  check to save some (1.5s)
  return: 1 - player can get, 0-player cannot get
*/
static int avpkt_cache_check_frames_reseved_enough(av_packet_cache_t *cache_ptr)
{
    int64_t current_ms;
    int frame_dur_pts = cache_ptr->queue_video.frame_dur_pts;
    int64_t frames_in = cache_ptr->queue_video.frames_in;
    int64_t frames_out = cache_ptr->queue_video.frames_out;
    play_para_t *player = (play_para_t *)cache_ptr->context;

    int64_t amstream_buf_ms = 0;
    int64_t keepframe_ms = 0;
    int64_t queue_left_ms = 0;
    int64_t enterkeepframes_ms = 0;

    if (1/*frame_dur_pts > 0*/) {
        if (frame_dur_pts <= 0) {
            frame_dur_pts = 90000 / 25;
        }

        current_ms = (int64_t)(player->state.current_ms);
        amstream_buf_ms = (int64_t)((frames_out * frame_dur_pts) / 90) -
                          (current_ms - (s_avpkt_cache.starttime_ms + s_avpkt_cache.discontinue_current_ms));
        enterkeepframes_ms = (int64_t)((cache_ptr->enterkeepframes * frame_dur_pts) / 90);
        if (s_avpkt_cache.startenterkeepframes == 0
            && amstream_buf_ms < enterkeepframes_ms) {
            //log_print("startenterkeepframes:%d, amstream_buf_ms:%lld, enterkeepframes_ms:%lld",
            //s_avpkt_cache.startenterkeepframes, amstream_buf_ms, enterkeepframes_ms);
            return 1;
        } else {
            if (s_avpkt_cache.startenterkeepframes == 0) {
                s_avpkt_cache.startenterkeepframes = 1;
            }
        }

        keepframe_ms = (int64_t)((cache_ptr->keepframes * frame_dur_pts) / 90);
        queue_left_ms = (int64_t)(((frames_in - frames_out) * frame_dur_pts) / 90);
        if (cache_ptr->resetkeepframes == 1) {
            //log_print("queue_left_ms:%lld, amstream_buf_ms:%lld, enterkeepframes_ms:%lld",
            //queue_left_ms, amstream_buf_ms, enterkeepframes_ms);
            if (queue_left_ms > 0
                && amstream_buf_ms < enterkeepframes_ms) {
                return 1;
            } else {
                cache_ptr->resetkeepframes = 0;
                cache_ptr->startenterkeepframes = 0;
                return 1;
            }
        } else {
            //log_print("startenterkeepframes:%d, queue_left_ms:%lld, keepframe_ms:%lld",
            //s_avpkt_cache.startenterkeepframes, queue_left_ms, keepframe_ms);
            if (s_avpkt_cache.startenterkeepframes == 1 && queue_left_ms > keepframe_ms) {
                return 1;
            } else {
                return 0;
            }
        }
    } else {
        if (frames_out <= cache_ptr->enterkeepframes) {
            return 1;
        }

        if (cache_ptr->resetkeepframes == 1) {
            if ((frames_in - frames_out) >= (int)(cache_ptr->keepframes / 2)) { //need modify ,keep 300 go not enough
                return 1;
            } else {
                cache_ptr->resetkeepframes = 0;
                return 0;
            }
        } else {
            if ((frames_in - frames_out) <= cache_ptr->keepframes) {
                return 0;
            } else {
                return 1;
            }
        }
    }

    return 1;
}

/*
return: 1 - sync finish, 0 - syncing
*/
#define DROP_VIDEO_MAX_FRAMES (5)
int avpkt_cache_avsync(av_packet_cache_t *cache_ptr)
{
    /*
    1.strong sync mode: wait both video and audio, when apts < vpts && diff_pts < configured_diff_pts, then sync ok
    */
    PacketQueue *q_video = NULL;
    PacketQueue *q_audio = NULL;
    int64_t keyframe_vpts = -1;
    int64_t keyframe_apts = -1;
    int64_t droppts = -1;

    int ret = 0;

    if (cache_ptr == NULL) {
        return 0;
    }

    if (cache_ptr->start_avsync_finish == 1) {
        return 1;
    }

    q_video = &cache_ptr->queue_video;
    q_audio = &cache_ptr->queue_audio;

    keyframe_vpts = q_video->dropref_pts;
    keyframe_apts = q_audio->dropref_pts;

    if (q_video->first_keyframe == -1
        && (keyframe_vpts == -1 || keyframe_apts == -1)) {
        return 0;
    }
#if 0
    if (keyframe_apts < 0) {
        //start play, video i frame has come,but no audio frame
        if (cache_ptr->start_avsync_finish == 0) {
            avpkt_cache_queue_seek_bypts(q_video, keyframe_vpts, 1);
            cache_ptr->start_avsync_finish = 2;
        }

        return cache_ptr->start_avsync_finish;
    }
#endif
    if (q_video->first_keyframe == 0) {
        cache_ptr->start_avsync_finish = 1;
        return cache_ptr->start_avsync_finish;
    } else if (q_video->first_keyframe == 1) {
        if (q_video->dropref_pts == AV_NOPTS_VALUE || q_audio->dropref_pts == AV_NOPTS_VALUE) {
            cache_ptr->start_avsync_finish = 1;
            log_print("%s first video/audio keyframe pts invalid\n", __FUNCTION__);
            return cache_ptr->start_avsync_finish;
        }
    }

    //log_print("strong avsync-keyframe_apts:0x%llx, keyframe_vpts:0x%llx, diff_pts:0x%llx",
    //keyframe_apts, keyframe_vpts, keyframe_vpts - keyframe_apts);

    int64_t drop_diffpts = 0;
    drop_diffpts = am_getconfig_int_def("libplayer.cache.dropdstpts", 9000);
    droppts = (int64_t)(am_getconfig_int_def("media.amplayer.sync_switch_ms", 1000) * 90);

    if (keyframe_apts > keyframe_vpts) {
        if (keyframe_apts - keyframe_vpts >= droppts) {
            cache_ptr->start_avsync_finish = 1;
        } else {
            if (cache_ptr->start_avsync_finish == 3) {
                if (q_video->frames_out >= DROP_VIDEO_MAX_FRAMES) {
                    cache_ptr->start_avsync_finish = 1;
                } else {
                    q_video->dropref_pts = q_video->cur_pkt->pkt.pts;
                }
            } else {
                //check whether audio seek back by search
                if ((ret = avpkt_cache_queue_search_bypts(q_audio, keyframe_vpts)) == 0) {
                    ret = avpkt_cache_queue_seek_bypts(q_audio, keyframe_vpts, 1);
                    cache_ptr->start_avsync_finish = 1;
                } else {
                    //drop video frames
                    log_print("drop-keyframe_apts:0x%llx, keyframe_vpts:0x%llx, diff_pts:0x%llx, vin:%lld, vout:%lld\n",
                              keyframe_apts, keyframe_vpts, keyframe_vpts - keyframe_apts, q_video->frames_in, q_video->frames_out);
                    ret = avpkt_cache_queue_dropframes(q_video, keyframe_apts, 0);
                    if (ret == 0) {
                        cache_ptr->start_avsync_finish = 1;
                    } else {
                        keyframe_vpts = q_video->cur_valid_pts;
                        if (keyframe_apts < keyframe_vpts) {
                            //drop audio frames
                            if (keyframe_vpts - keyframe_apts <= drop_diffpts) {
                                cache_ptr->start_avsync_finish = 1;
                            } else {
                                ret = avpkt_cache_queue_dropframes(q_audio, keyframe_vpts, 1);
                                if (ret == 0) {
                                    cache_ptr->start_avsync_finish = 1;
                                } else {
                                    keyframe_apts = q_audio->cur_valid_pts;
                                    if (keyframe_vpts - keyframe_apts <= drop_diffpts) {
                                        cache_ptr->start_avsync_finish = 1;
                                    } else {
                                        cache_ptr->start_avsync_finish = 2;//need  drop audio continue
                                    }
                                }
                            }
                        } else {
                            cache_ptr->start_avsync_finish = 3;//need  first output video continue
                            q_video->dropref_pts = q_video->cur_pkt->pkt.pts;
                            log_print("start_avsync_finish=3, dropvpts:0x%llx, frame_out:%lld\n", q_video->dropref_pts, q_video->frames_out);
                        }
                    }
                }
            }
        }
    } else if (keyframe_apts < keyframe_vpts) {
        //drop audio frame
        /*
            if call drop frames this time, but apts still is less than vpts,
            should let video frame out for quick show first video, then still drop,
            compare with current play pts
        */
        if (cache_ptr->start_avsync_finish == 3 || keyframe_vpts - keyframe_apts <= drop_diffpts || keyframe_vpts - keyframe_apts >= droppts) {
            cache_ptr->start_avsync_finish = 1;
        } else {
            ret = avpkt_cache_queue_dropframes(q_audio, keyframe_vpts, 1);
            if (ret == 0) {
                cache_ptr->start_avsync_finish = 1;
            } else {
                keyframe_apts = q_audio->cur_valid_pts;
                if (keyframe_vpts - keyframe_apts <= drop_diffpts) {
                    cache_ptr->start_avsync_finish = 1;
                } else {
                    cache_ptr->start_avsync_finish = 2;//need  drop audio continue
                }
            }
        }
    } else {
        cache_ptr->start_avsync_finish = 1;
    }

    return cache_ptr->start_avsync_finish;
}

int avpkt_cache_get(AVPacket *pkt)
{
    int stream_idx = -1;
    int ret = 0;

    int netdown = 0;
    int netdown_last = s_avpkt_cache.last_netdown_state;

    int64_t keyframe_vpts = -1;
    int64_t keyframe_apts = -1;
    int get_ok = 0;
    play_para_t *player = (play_para_t *)s_avpkt_cache.context;

#ifdef DEBUT_PUT_GET
    if (get_cnt % 20 == 0) {
        log_print("\n%s get--- out:%lld, forward:%d, backward:%d\n",
                  __FUNCTION__, s_avpkt_cache.queue_video.frames_out,
                  s_avpkt_cache.queue_video.frames_for_seek_forward,
                  s_avpkt_cache.queue_video.frames_for_seek_backward);
    }
    get_cnt++;
#endif

    while (1) {
        if (s_avpkt_cache.state != 2) {
            break;
        }

        if (/*s_avpkt_cache.audio_count ==0 && */s_avpkt_cache.video_count == 0) {
            break;
        }

        if (s_avpkt_cache.trickmode == 1 && s_avpkt_cache.queue_video.frames_for_seek_forward <= 0) {
            amthreadpool_thread_usleep(10 * 1000);
            break;
        }

        if (s_avpkt_cache.need_avsync == 1 && avpkt_cache_avsync(&s_avpkt_cache) == 0) {
            amthreadpool_thread_usleep(10 * 1000);
            break;
        }

        if ((ret = avpkt_cache_check_can_get(&s_avpkt_cache, &stream_idx)) == 1) {
            if (s_avpkt_cache.enable_keepframes == 1) {
                netdown = avpkt_cache_check_netlink();
                {
                    if (s_avpkt_cache.error != 0
                        && s_avpkt_cache.error != AVERROR(EAGAIN)) {
                        //go to get, maybe eof
                        ret = s_avpkt_cache.error;
                    } else {
                        if (avpkt_cache_check_frames_reseved_enough(&s_avpkt_cache) == 0) {
                            if (netdown_last == 1 && netdown == 0) {
                                //net down ->net up
                                s_avpkt_cache.resetkeepframes = 1;
                            } else {
                                ret = AVERROR(EAGAIN);
                                break;
                            }
                        }
                    }
                }

                if (netdown != netdown_last) {
                    s_avpkt_cache.last_netdown_state = s_avpkt_cache.netdown;
                }

            }

            /*
            after do avsync , if vpts -apts is still bigger than configured_pts, in order to let video shown for quick first pic,
            if  current audio pkt frame_id is less than current video pkt frame_id, change to video pkt
            */
            if (s_avpkt_cache.need_avsync == 1 && s_avpkt_cache.start_avsync_finish > 1) {
                stream_idx = s_avpkt_cache.video_index;
            }
            //end

            if (s_avpkt_cache.trickmode == 1
                && s_avpkt_cache.fffb_start == 1) {
                if (s_avpkt_cache.fffb_out_frames == 0) {
                    get_ok = 1;
                    ret = 0;
                    player->playctrl_info.no_need_more_data = 1;

#ifdef DEBUT_PUT_GET
                    log_print("%s --- out:%lld, forward:%d, backward:%d, cur_id:%lld, last_id:%lld\n",
                              __FUNCTION__, s_avpkt_cache.queue_video.frames_out,
                              s_avpkt_cache.queue_video.frames_for_seek_forward,
                              s_avpkt_cache.queue_video.frames_for_seek_backward,
                              s_avpkt_cache.queue_video.cur_pkt->frame_id,
                              s_avpkt_cache.queue_video.last_pkt->frame_id);
#endif

                    break;
                }

#ifdef DEBUT_PUT_GET
                log_print("%s out:%lld, forward:%d, backward:%d, cur_id:%lld, last_id:%lld\n",
                          __FUNCTION__, s_avpkt_cache.queue_video.frames_out,
                          s_avpkt_cache.queue_video.frames_for_seek_forward,
                          s_avpkt_cache.queue_video.frames_for_seek_backward,
                          s_avpkt_cache.queue_video.cur_pkt->frame_id,
                          s_avpkt_cache.queue_video.last_pkt->frame_id);
#endif

                player->playctrl_info.no_need_more_data = 0;
                s_avpkt_cache.fffb_out_frames--;
            }

            ret = avpkt_cache_get_byindex(&s_avpkt_cache, pkt, stream_idx);
            if (ret == -1) {
                if (s_avpkt_cache.error != AVERROR(EAGAIN)) {
                    ret = s_avpkt_cache.error;
                    log_print("avpkt_cache_get_byindex fail, error:%d\n", ret);
                } else {
                    //ret = AVERROR(EAGAIN);
                    ret = 0;
                }

                break;
            } else {
                get_ok = 1;
                break;
            }
        } else {
            break;
        }
    }

    if (get_ok == 0) {
        if (s_avpkt_cache.queue_video.frames_for_seek_forward <= 0
            && s_avpkt_cache.error != 0) {
            ret = s_avpkt_cache.error;
        }
    }

    return ret;
}

int avpkt_cache_get_netlink(void)
{
    return s_avpkt_cache.netdown;
}

static int avpkt_cache_check_netlink(void)
{
    if (s_avpkt_cache.local_play == 1) {
        return 0;
    }

    char acNetStatus[PROPERTY_VALUE_MAX] = {0};
    //net.eth0.hw.status
    property_get("net.eth0.hw.status", acNetStatus, NULL);
    if (acNetStatus[0] == 'c') {
        s_avpkt_cache.netdown = 0;
    } else if (acNetStatus[0] == 'd') {
        s_avpkt_cache.netdown = 1;
    }

    if (s_avpkt_cache.netdown == 1) {
        memset(acNetStatus, 0x0, sizeof(acNetStatus));
        property_get("net.ethwifi.up", acNetStatus, NULL);
        if (atoi(acNetStatus) > 0) {
            s_avpkt_cache.netdown = 0;
        }
    }
    return s_avpkt_cache.netdown;
}

static int avpkt_cache_put(void)
{
    play_para_t *player = (play_para_t *)s_avpkt_cache.context;
    AVPacket pkt;
    int ret = 0;

    if ((ret = avpkt_cache_check_can_put(&s_avpkt_cache)) == 0) {
        return -1;
    }

    av_init_packet(&pkt);
    s_avpkt_cache.reading = 1;

    ret = av_read_frame(player->pFormatCtx, &pkt);
    if (s_avpkt_cache.state != 2) {
        log_print("av_read_frame ret:%d", ret);
    }

#ifdef DEBUT_PUT_GET
    if (ret >= 0) {
        if (put_cnt >= 0 && (put_cnt % 20 == 0)) {
            log_print("\n%s put--- out:%lld, forward:%d, backward:%d\n",
                      __FUNCTION__, s_avpkt_cache.queue_video.frames_out,
                      s_avpkt_cache.queue_video.frames_for_seek_forward,
                      s_avpkt_cache.queue_video.frames_for_seek_backward);

        }
        put_cnt++;
    }
#endif

    s_avpkt_cache.error = ret;
    if (ret < 0) {
        //log_print("cache one frame failed, ret:%d cmd:%d, state:%d\n", ret, s_avpkt_cache.cmd, s_avpkt_cache.state);
        if (ret == AVERROR_EOF) {
            log_print("read eof !");//if eof ,should not read again?
        }
    } else {
        for (; ;) {
            if (s_avpkt_cache.state != 2) {
                ret = -1;
                break;
            }
            ret = avpkt_cache_put_update(&s_avpkt_cache, &pkt);
            if (ret == 0) {
                break;
            } else if (ret == -2) {
                //not find first video keyframe
                break;
            } else if (ret == -1) {
                amthreadpool_thread_usleep(10 * 1000);
            }
        }
    }

    s_avpkt_cache.reading = 0;
    if (ret >= 0 || ret == -2) {
        s_avpkt_cache.read_frames++;
    }

    if (pkt.size > 0) {
        av_free_packet(&pkt);
    }

    av_init_packet(&pkt);

    return ret;
}

void *cache_worker(void *arg)
{
    int64_t diff_ms;
    int64_t last_current_ms = 0;
    int64_t starttime_us = 0;
    int64_t curtime_us = 0;
    int64_t current_ms = 0;
    play_para_t *player = (play_para_t*)arg;
    avpkt_cache_init(&s_avpkt_cache, (void *)player);
    int nRunning = 1;
    int ret = 0;
    int64_t read_frames = 20;

    s_avpkt_cache.state = 2;
    while (nRunning == 1) {
        if (s_avpkt_cache.state == 2) {
            if (player->playctrl_info.pause_cache != 0) {
                amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                continue;
            }

            //update bufed_time
            player->state.bufed_time = packet_update_bufed_time();
            //end

            if (s_avpkt_cache.error == AVERROR_EOF) {
                amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                continue;
            }

            if (avpkt_cache_check_netlink() == 1) {
                //net down, sleep
                amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                continue;
            }

            if ((s_avpkt_cache.trickmode == 1) && s_avpkt_cache.queue_video.frames_for_seek_forward >= 61) {
                amthreadpool_thread_usleep(5 * CACHE_THREAD_SLEEP_US);
            }

            if ((ret = avpkt_cache_put()) < 0) {
                if (s_avpkt_cache.state == 2) {
                    if (ret == -2) {
                        //drop audio,continue read frames
                        //amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                    } else {
                        if (ret != AVERROR(EAGAIN)
                            || (avpkt_cache_check_netlink() == 1)) {
                            //may be eof
                            //may be netdown
                            amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                        } else {
                            //check decoder cache time,sleep some microsencond to avoid hold cpu
                            if (avpkt_cache_checkvlevel(&s_avpkt_cache, 0.5) > 0) {
                                amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                            } else {
                                if (avpkt_cache_check_netlink() == 1) {
                                    amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                                }
                            }
                            //end
                        }
                    }
                }
            } else {
                if (s_avpkt_cache.discontinue_current_ms_checked == 0
                    && s_avpkt_cache.discontinue_current_ms == 0) {
                    current_ms = (int64_t)(player->state.current_ms);
                    if (s_avpkt_cache.starttime_ms) {
                        diff_ms = (current_ms - s_avpkt_cache.starttime_ms);
                    } else {
                        diff_ms = (current_ms - s_avpkt_cache.last_currenttime_ms);
                    }
                    if (diff_ms != 0) {
                        if (abs(diff_ms) >= CURRENT_TIME_MS_DISCONTINUE) {
                            s_avpkt_cache.discontinue_current_ms_flag = 1;
                            s_avpkt_cache.discontinue_current_ms = diff_ms;
                            log_print("%s seektime:%lld, starttime:%lld, currenttime:%lld, disc_ms:%lld\n",
                                      __FUNCTION__, s_avpkt_cache.seekTimeMs, s_avpkt_cache.starttime_ms, current_ms, abs(diff_ms));
                        }
                        s_avpkt_cache.currenttime_ms = current_ms;
                        s_avpkt_cache.last_currenttime_ms = current_ms;

                        s_avpkt_cache.discontinue_current_ms_checked = 1;
                    }
                }
            }

            /*other function*/
            if (avpkt_cache_checkvlevel(&s_avpkt_cache, 0.5) > 0) {
                read_frames--;
                if (read_frames == 0) {
                    amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
                    read_frames = 20;
                }
            }
            /*end*/
            continue;
        } else if (s_avpkt_cache.state == 0) {
            break;
        } else if (s_avpkt_cache.state == 1) {
            last_current_ms = 0;
            current_ms = 0;
            curtime_us = 0;
            starttime_us = 0;
            amthreadpool_thread_usleep(CACHE_THREAD_SLEEP_US);
        }
    }

    avpkt_cache_release(&s_avpkt_cache);
    log_print("%s:%d end\n", __FUNCTION__, __LINE__);
    return NULL;
}

int avpkt_cache_task_open(play_para_t *player)
{
    int ret = 0;
    pthread_t       tid;
    pthread_attr_t pthread_attr;

    pthread_attr_init(&pthread_attr);
    pthread_attr_setstacksize(&pthread_attr, 0);   //default stack size maybe better
    log_print("open avpacket cache worker\n");

    ret = amthreadpool_pthread_create(&tid, &pthread_attr, (void*)&cache_worker, (void*)player);
    if (ret != 0) {
        log_print("creat player thread failed !\n");
        return ret;
    }

    log_print("[avpkt_cache_task_open:%d]creat cache thread success,tid=%lu\n", __LINE__, tid);
    pthread_setname_np(tid, "AVPacket_Cache");
    player->cache_thread_id = tid;
    pthread_attr_destroy(&pthread_attr);

    return PLAYER_SUCCESS;
}

int avpkt_cache_task_close(play_para_t *player)
{
    int ret = 0;
    if (player->cache_thread_id != 0) {
        log_print("[%s:%d]start join cache thread,tid=%lu\n", __FUNCTION__, __LINE__, player->cache_thread_id);
        //s_avpkt_cache.cmd = CACHE_CMD_STOP;
        avpkt_cache_set_cmd(CACHE_CMD_STOP);
        ret = amthreadpool_pthread_join(player->cache_thread_id, NULL);
    }

    log_print("[%s:%d]join cache thread tid=%lu, ret=%d\n", __FUNCTION__, __LINE__, player->cache_thread_id, ret);
    return ret;
}

