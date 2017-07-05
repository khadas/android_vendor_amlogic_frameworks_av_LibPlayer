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



#ifndef PLAYER_FFMPEG_CACHE_H
#define PLAYER_FFMPEG_CACHE_H

#include "player_para.h"
//#include "player_priv.h"


#define cache_lock_t         pthread_mutex_t
#define cache_lock_init(x,v) pthread_mutex_init(x,v)
#define cache_lock_uninit(x) pthread_mutex_destroy(x)
#define cache_lock(x)        pthread_mutex_lock(x)
#define cache_unlock(x)      pthread_mutex_unlock(x)

typedef enum {
    CACHE_CMD_START = 0x1,
    CACHE_CMD_SEEK_IN_CACHE = 0x2,
    CACHE_CMD_SEEK_OUT_OF_CACHE = 0x3,
    CACHE_CMD_STOP = 0x04,
    CACHE_CMD_SEARCH_OK = 0x5,
    CACHE_CMD_SEARCH_START = 0x6,
    CACHE_CMD_RESET = 0x7,
    CACHE_CMD_RESET_OK = 0x8,
    CACHE_CMD_FFFB = 0x9,
    CACHE_CMD_FFFB_OK = 0xa
} AVPacket_Cache_E;

typedef struct MyAVPacketList {
    AVPacket pkt;
    int64_t frame_id;//该pkt进入队列的编号,由外部av_packet_cache_t游标指定.
    int64_t offset_pts;//从cache被创建开始到当前包的总pts差值
    int used;//this pkt has been read

    //for seek
    int64_t pts;// pkt->pts or pkt->dts
    int64_t pts_discontinue;
    //end

    struct MyAVPacketList *next;
    struct MyAVPacketList *priv;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt;
    MyAVPacketList *last_pkt;
    MyAVPacketList *cur_pkt;//point to next cache frame for codec_write

    int stream_index;
    int nb_packets;
    int size;//total malloc size of all packets
    int backwardsize;//data size for seek backward
    int forwardsize;//data size for seek forward

    cache_lock_t lock;

    int max_packets;//max cache frames support in list
    int queue_max_kick;//the first time come to max packet num
    int64_t queue_maxtime_pts;

    int64_t first_valid_pts;
    int64_t head_valid_pts;
    int64_t tail_valid_pts;
    int64_t cur_valid_pts;

    int64_t pts1;
    int64_t pts2;
    int64_t last_pts2;
    int64_t discontinue_pts;
    int discontinue_flag;
    int64_t dur_calc_pts_start;
    int64_t dur_calc_pts_end;
    int64_t dur_calc_cnt;
    int64_t cache_pts;
    int64_t bak_cache_pts;
    int dur_calc_done;
    int frame_dur_pts;

    int64_t firstPkt_playtime_pts;
    int64_t curPkt_playtime_pts;
    int64_t lastPkt_playtime_pts;

    int64_t frames_in;
    int64_t frames_out;

    int trust_keyframe;//trust keyframe pts flag
    int64_t first_keyframe_pts;
    int64_t dropref_pts;//drop audio refer vpts
    int first_keyframe;//-1 -initial, 0-not found, 1-found
    int keyframe_check_cnt;
    int keyframe_check_max;//refer video

    //for cache seek
    int frames_max_seekbackword;
    int frames_for_seek_backward;
    int frames_max_seekforword;
    int frames_for_seek_forward;
    float frames_backward_level;
    //end
    //timebase
    float timebase;
} PacketQueue;

typedef struct av_packet_cache {
    int enable_seek_in_cache;
    int reading;
    int state; //0-not inited, 1-inited(wait for can/get/peek cmd),  2-can put,get,peek
    int netdown;
    int last_netdown_state;
    int cmd;

    int has_audio;
    int has_video;
    int has_sub;

    int audio_count;
    int video_count;
    int sub_count;

    int audio_size;
    int video_size;
    int sub_size;

    int audio_index;
    int video_index;
    int sub_index;

    int64_t seekTimeMs;
    int64_t first_apts;
    int64_t first_vpts;
    int64_t first_spts;

    PacketQueue queue_audio;
    PacketQueue queue_video;
    PacketQueue queue_sub;

    int64_t video_cachems;
    int64_t audio_cachems;
    int64_t sub_cachems;

    int error;//av_read_frame error code
    int64_t read_frames;//从表创建开始到当前时间总read的帧数
    int max_cache_mem;
    int max_packet_num;

    int audio_max_packet;
    int video_max_packet;
    int sub_max_packet;

    int64_t last_currenttime_ms;
    int64_t currenttime_ms;
    int64_t starttime_ms;
    int64_t discontinue_current_ms;//occur after seek ( current_ms < seekTimeMs -3)
    int discontinue_current_ms_flag;
    int discontinue_current_ms_checked;

    int enable_keepframes;
    int enterkeepframes;//ms, not check frames when out frames is small than this value
    int startenterkeepframes;
    int keepframes;
    int resetkeepframes;

    int start_avsync_finish;
    int need_avsync;
    int search_by_keyframe;
    int trickmode;//ff/fb
    int fffb_start;
    int fffb_out_frames;
    int pause_cache;
    int local_play;
    void *context;
} av_packet_cache_t;

struct play_para;

int avpkt_cache_task_open(struct play_para *player);
int avpkt_cache_task_close(struct play_para *player);
int avpkt_cache_get(AVPacket *pkt);
int avpkt_cache_search(struct play_para *player, int64_t seekTimeSec);
int64_t avpkt_cache_getcache_time_by_streamindex(struct play_para *player, int stream_idx);
int avpkt_cache_set_cmd(AVPacket_Cache_E cmd);
int avpkt_cache_get_netlink(void);
int64_t avpkt_cache_getcache_time(struct play_para *player, int stream_idx);


#endif
