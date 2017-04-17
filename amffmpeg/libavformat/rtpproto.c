/*
 * RTP network protocol
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * RTP protocol
 */

#include "libavutil/parseutils.h"
#include "libavutil/avstring.h"
#include "avformat.h"
#include "avio_internal.h"
#include "rtpdec.h"
#include "url.h"

#include <unistd.h>
#include <stdarg.h>
#include "internal.h"
#include "network.h"
#include "os_support.h"
#include <fcntl.h>
#if HAVE_POLL_H
#include <sys/poll.h>
#endif
#include <sys/time.h>
#include "libavcodec/get_bits.h"
#include <amthreadpool.h>
#include <itemlist.h>
#include "RS_fec.h"

#define RTP_TX_BUF_SIZE  (64 * 1024)
#define RTP_RX_BUF_SIZE  (128 * 1024)
#define RTPPROTO_RECVBUF_SIZE 3 * RTP_MAX_PACKET_LENGTH
#define MIN_CACHE_PACKET_SIZE 5

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#define FEC_RECVBUF_SIZE 3000
#define MAX_FEC_RTP_PACKET_NUM 300
#define MAX_FEC_PACKET_NUM 10
#define MAX_FEC_MAP_NUM 310

#define EXTRA_BUFFER_PACKET_NUM 20

#define FORCE_OUTPUT_PACKET_NUM_THRESHOLD 1000
#define FORCE_OUTPUT_PACKET_NUM 20
#define FEC_PAYLOAD_TYPE 127
typedef struct FEC_DATA_STRUCT {
    uint16_t rtp_begin_seq;
    uint16_t rtp_end_seq;
    uint8_t redund_num;
    uint8_t redund_idx;
    uint16_t fec_len;
    uint16_t rtp_len;
    uint16_t rsv;
    uint8_t *fec_data;                  // point to rtp buffer
} FEC_DATA_STRUCT;

typedef struct RTPFECPacket {
    uint16_t seq;
    uint8_t payload_type;
    uint8_t *buf;                       //recv buffer
    int len;

    FEC_DATA_STRUCT * fec;          // fec struct
} RTPFECPacket;

typedef struct RTPFECContext {
    URLContext *rtp_hd, *fec_hd;
    int rtp_fd, fec_fd;

    volatile uint8_t brunning;
    pthread_t recv_thread;

    uint8_t bdecode;
    struct itemlist recvlist;
    struct itemlist outlist;
    struct itemlist feclist;
    /*
        RTPFECPacket *fec_packet[MAX_FEC_PACKET_NUM];
        RTPFECPacket *rtp_packet[MAX_FEC_RTP_PACKET_NUM];

        uint8_t *fec_data_array[MAX_FEC_PACKET_NUM];
        uint8_t *rtp_data_array[MAX_FEC_RTP_PACKET_NUM];
        uint8_t lost_map[MAX_FEC_MAP_NUM];
    */
    FEC_DATA_STRUCT * cur_fec;
    uint16_t rtp_last_decode_seq;
    uint16_t rtp_media_packet_sum;
    uint8_t rtp_seq_discontinue;
    uint8_t fec_seq_discontinue;

    T_RS_FEC_MONDE *fec_handle;
} RTPFECContext;

static RTPFECPacket *fec_packet[MAX_FEC_PACKET_NUM];
static RTPFECPacket *rtp_packet[MAX_FEC_RTP_PACKET_NUM];

static PBYTE fec_data_array[MAX_FEC_PACKET_NUM];
static PBYTE rtp_data_array[MAX_FEC_RTP_PACKET_NUM];
static int lost_map[MAX_FEC_MAP_NUM];

//#define TRACE() av_log(NULL, AV_LOG_INFO, "[%s:%d]\n", __FUNCTION__, __LINE__);
#define TRACE()

/**
 * If no filename is given to av_open_input_file because you want to
 * get the local port first, then you must call this function to set
 * the remote server address.
 *
 * @param h media file context
 * @param uri of the remote server
 * @return zero if no error.
 */
static int gd_report_error_enable = 0;
#define PLAYER_EVENTS_ERROR 3

static int init_def_settings()
{
    static int inited = 0;
    if (inited > 0) {
        return 0;
    }
    inited++;
    gd_report_error_enable = (int)am_getconfig_bool_def("media.player.gd_report.enable", 0);
    av_log(NULL, AV_LOG_ERROR, "udp config: gd_report enable:%d\n\n", gd_report_error_enable);
    return 0;
}

int rtp_set_remote_url(URLContext *h, const char *uri)
{
    RTPContext *s = h->priv_data;
    char hostname[256];
    int port;

    char buf[1024];
    char path[1024];

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &port,
                 path, sizeof(path), uri);

    ff_url_join(buf, sizeof(buf), "udp", NULL, hostname, port, "%s", path);
    ff_udp_set_remote_url(s->rtp_hd, buf);

    ff_url_join(buf, sizeof(buf), "udp", NULL, hostname, port + 1, "%s", path);
    ff_udp_set_remote_url(s->rtcp_hd, buf);
    return 0;
}


/**
 * add option to url of the form:
 * "http://host:port/path?option1=val1&option2=val2...
 */

static void url_add_option(char *buf, int buf_size, const char *fmt, ...)
{
    char buf1[1024];
    va_list ap;

    va_start(ap, fmt);
    if (strchr(buf, '?')) {
        av_strlcat(buf, "&", buf_size);
    } else {
        av_strlcat(buf, "?", buf_size);
    }
    vsnprintf(buf1, sizeof(buf1), fmt, ap);
    av_strlcat(buf, buf1, buf_size);
    va_end(ap);
}

static void build_udp_url(char *buf, int buf_size,
                          const char *hostname, int port,
                          int local_port, int ttl,
                          int max_packet_size, int connect, int setbufsize)
{
    ff_url_join(buf, buf_size, "udp", NULL, hostname, port, NULL);
    if (local_port >= 0) {
        url_add_option(buf, buf_size, "localport=%d", local_port);
    }
    if (ttl >= 0) {
        url_add_option(buf, buf_size, "ttl=%d", ttl);
    }
    if (max_packet_size >= 0) {
        url_add_option(buf, buf_size, "pkt_size=%d", max_packet_size);
    }
    if (connect) {
        url_add_option(buf, buf_size, "connect=1");
    }
    if (setbufsize > 0) {
        url_add_option(buf, buf_size, "buffer_size=655360");
    }

    url_add_option(buf, buf_size, "fifo_size=0");
}

#define MAX_RTP_SEQ 65536
#define MAX_RTP_SEQ_SPAN 60000
static int seq_greater(int first, int second)
{
    if (first == second) {
        return 0;
    } else if (abs(first - second) > MAX_RTP_SEQ_SPAN) {
        if (first < second) {
            return 1;
        } else {
            return 0;
        }
    } else if (first > second) {
        return 1;
    } else {
        return 0;
    }

}

static int seq_less(int first, int second)
{
    if (first == second) {
        return 0;
    } else if (abs(first - second) > MAX_RTP_SEQ_SPAN) {
        if (first > second) {
            return 1;
        } else {
            return 0;
        }
    } else if (first < second) {
        return 1;
    } else {
        return 0;
    }

}

static int seq_greater_and_equal(int first, int second)
{
    if (first == second) {
        return 1;
    } else {
        return seq_greater(first, second);
    }
}

static int seq_less_and_equal(int first, int second)
{
    if (first == second) {
        return 1;
    } else {
        return seq_less(first, second);
    }
}

static int seq_subtraction(int first, int second)
{
    if (first == second) {
        return 0;
    } else if (abs(first - second) > MAX_RTP_SEQ_SPAN && first < second) {
        return first + MAX_RTP_SEQ - second;
    } else {
        return first - second;
    }
}

static int rtp_free_packet(void * apkt)
{
    RTPPacket * lpkt = apkt;
    if (lpkt != NULL) {
        if (lpkt->buf != NULL) {
            av_free(lpkt->buf);
        }
        av_free(lpkt);
    }
    apkt = NULL;
    return 0;
}

static int inner_rtp_read(RTPContext *s, uint8_t *buf, int size, URLContext* h)
{
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    int64_t starttime = ff_network_gettime();
    int64_t curtime;
    struct pollfd p[2] = {{s->rtp_fd, POLLIN, 0}, {s->rtcp_fd, POLLIN, 0}};

    for (;;) {
        if (url_interrupt_cb()) {
            return AVERROR_EXIT;
        }
        /* build fdset to listen to RTP and RTCP packets */
        n = poll(p, 2, 100);
        if (n > 0) {
            /* first try RTCP */
            if (p[1].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom(s->rtcp_fd, buf, size, 0,
                               (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR)) {
                        continue;
                    }
                    return AVERROR(EIO);
                }
                break;
            }
            /* then RTP */
            if (p[0].revents & POLLIN) {
                starttime = 0;
                s->report_flag = 0;
                from_len = sizeof(from);
                len = recvfrom(s->rtp_fd, buf, size, 0,
                               (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR)) {
                        continue;
                    }
                    return AVERROR(EIO);
                }
                break;
            }
        } else if (n < 0) {
            if (ff_neterrno() == AVERROR(EINTR)) {
                continue;
            }
            return AVERROR(EIO);
        }
        curtime = ff_network_gettime();
        if (starttime <= 0) {
            starttime = curtime;
        }
        if (gd_report_error_enable && curtime > starttime + 30 * 1000 * 1000 && !s->report_flag) {
            s->report_flag = 1;
            ffmpeg_notify(h, PLAYER_EVENTS_ERROR, 54000, 0);
        }
    }

    return len;
}

static int inner_rtp_read1(RTPContext *s, uint8_t *buf, int size)
{
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[1] = {{s->rtp_fd, POLLIN, 0}};

    for (;;) {
        if (url_interrupt_cb()) {
            return AVERROR_EXIT;
        }
        /* build fdset to listen to only RTP packets */
        n = poll(p, 1, 100);
        if (n > 0) {
            /* then RTP */
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom(s->rtp_fd, buf, size, 0,
                               (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR)) {
                        continue;
                    }
                    return AVERROR(EIO);
                }
                break;
            }
        } else if (n < 0) {
            if (ff_neterrno() == AVERROR(EINTR)) {
                continue;
            }
            return AVERROR(EIO);
        } else {
            return AVERROR(EAGAIN);
        }
    }


    return len;
}


static int rtp_enqueue_packet(struct itemlist *itemlist, RTPPacket * lpkt)
{
    RTPPacket *ltailpkt = NULL;
    itemlist_peek_tail_data(itemlist, (unsigned long *)&ltailpkt) ;
    if (NULL == ltailpkt || (ltailpkt != NULL && seq_less(ltailpkt->seq, lpkt->seq) == 1)) {
        // append to the tail
        itemlist_add_tail_data(itemlist, (unsigned long)lpkt) ;
        return 0;
    }

    RTPPacket *lheadpkt = NULL;
    itemlist_peek_head_data(itemlist, (unsigned long*)&lheadpkt) ;
    ITEM_LOCK(itemlist);
    if (itemlist->item_count >= MIN_CACHE_PACKET_SIZE && lheadpkt != NULL && seq_greater(lheadpkt->seq, lpkt->seq) == 1) {
        ITEM_UNLOCK(itemlist);
        av_log(NULL, AV_LOG_INFO, "[%s:%d]Out of range, seq=%d,headseq=%d\n", __FUNCTION__, __LINE__, lpkt->seq, lheadpkt->seq);
        rtp_free_packet((void *)lpkt);
        lpkt = NULL;
        return 0;
    }
    ITEM_UNLOCK(itemlist);

    // insert to the queue
    struct item *item = NULL;
    struct item *newitem = NULL;
    struct list_head *llist = NULL, *tmplist = NULL;
    RTPPacket *llistpkt = NULL;

    ITEM_LOCK(itemlist);
    list_for_each_safe(llist, tmplist, &itemlist->list) {
        item = list_entry(llist, struct item, list);
        llistpkt = (RTPPacket *)(item->item_data);
        if (lpkt->seq == llistpkt->seq) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]The Replication packet, seq=%d\n", __FUNCTION__, __LINE__, lpkt->seq);
            rtp_free_packet((void *)lpkt);
            lpkt = NULL;
            break;
        } else if (seq_less(lpkt->seq, llistpkt->seq) == 1) {
            // insert to front
            newitem = item_alloc(itemlist->item_ext_buf_size);
            if (newitem == NULL) {
                ITEM_UNLOCK(itemlist);
                return -12;//noMEM
            }
            newitem->item_data = (unsigned long)lpkt;

            list_add_tail(&(newitem->list), &(item->list));
            itemlist->item_count++;
            break;
        }
    }
    ITEM_UNLOCK(itemlist);

    return 0;
}

static void *rtp_recv_task(void *_RTPContext)
{
    av_log(NULL, AV_LOG_INFO, "[%s:%d]rtp recv_buffer_task start running!!!\n", __FUNCTION__, __LINE__);
    RTPContext * s = (RTPContext *)_RTPContext;
    if (NULL == s) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d]Null handle!!!\n", __FUNCTION__, __LINE__);
        goto rtp_thread_end;
    }

    RTPPacket * lpkt = NULL;
    int datalen = 0 ;
    int payload_type = 0;

    uint8_t * lpoffset = NULL;
    int offset = 0;
    uint8_t * lpkt_buf = NULL;
    int len = 0;
    int ext = 0;

    while (s->brunning > 0) {
        if (url_interrupt_cb()) {
            goto rtp_thread_end;
        }

        if (lpkt != NULL) {
            rtp_free_packet((void *)lpkt);
            lpkt = NULL;
        }

        // malloc the packet buffer
        lpkt = av_mallocz(sizeof(RTPPacket));
        if (NULL == lpkt) {
            goto rtp_thread_end;
        }
        lpkt->buf = av_malloc(RTPPROTO_RECVBUF_SIZE);
        if (NULL == lpkt->buf) {
            goto rtp_thread_end;
        }

        // recv data
        lpkt->len = inner_rtp_read1(s, lpkt->buf, RTPPROTO_RECVBUF_SIZE);
        if (lpkt->len <= 12) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]receive wrong packet len=%d \n", __FUNCTION__, __LINE__, lpkt->len);
            amthreadpool_thread_usleep(10);
            continue;
        }
        // paser data and buffer the packat
        payload_type = lpkt->buf[1] & 0x7f;
        lpkt->seq = AV_RB16(lpkt->buf + 2);

        if (lpkt->len < 1000) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]receive short packet len=%d seq=%d\n", __FUNCTION__, __LINE__, lpkt->len, lpkt->seq);
        }

        if (payload_type == 33) {    // mpegts packet
            //av_log(NULL, AV_LOG_ERROR, "[%s:%d]mpegts packet req = %d\n", __FUNCTION__, __LINE__, lpkt->seq);
            // parse the rtp playload data
            lpkt_buf = lpkt->buf;
            len = lpkt->len;

            if (lpkt_buf[0] & 0x20) {                   // remove the padding data
                int padding = lpkt_buf[len - 1];
                if (len >= 12 + padding) {
                    len -= padding;
                }
            }

            if (len <= 12) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d]len<=12,len=%d\n", __FUNCTION__, __LINE__, len);
                continue;
            }

            // output the playload data
            offset = 12 ;
            lpoffset = lpkt_buf + 12;

            ext = lpkt_buf[0] & 0x10;
            if (ext > 0) {
                if (len < offset + 4) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < offset + 4\n", __FUNCTION__, __LINE__);
                    continue;
                }

                ext = (AV_RB16(lpoffset + 2) + 1) << 2;
                if (len < ext + offset) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < ext + offset\n", __FUNCTION__, __LINE__);
                    continue;
                }
                offset += ext ;
                lpoffset += ext ;
            }
            lpkt->valid_data_offset = offset;

            if (rtp_enqueue_packet(&(s->recvlist), lpkt) < 0) {
                goto rtp_thread_end;
            }
        } else {
            av_log(NULL, AV_LOG_ERROR, "[%s:%d]unknow payload type = %d, seq=%d\n", __FUNCTION__, __LINE__, payload_type, lpkt->seq);
            continue;
        }

        lpkt = NULL;
    }

rtp_thread_end:
    s->brunning = 0;
    av_log(NULL, AV_LOG_ERROR, "[%s:%d]rtp recv_buffer_task end!!!\n", __FUNCTION__, __LINE__);
    return NULL;
}

/**
 * url syntax: rtp://host:port[?option=val...]
 * option: 'ttl=n'            : set the ttl value (for multicast only)
 *         'rtcpport=n'       : set the remote rtcp port to n
 *         'localrtpport=n'   : set the local rtp port to n
 *         'localrtcpport=n'  : set the local rtcp port to n
 *         'pkt_size=n'       : set max packet size
 *         'connect=0/1'      : do a connect() on the UDP socket
 * deprecated option:
 *         'localport=n'      : set the local port to n
 *
 * if rtcpport isn't set the rtcp port will be the rtp port + 1
 * if local rtp port isn't set any available port will be used for the local
 * rtp and rtcp ports
 * if the local rtcp port is not set it will be the local rtp port + 1
 */


static int rtp_open(URLContext *h, const char *uri, int flags)
{
    RTPContext *s;
    int rtp_port, rtcp_port,
        ttl, connect,
        local_rtp_port, local_rtcp_port, max_packet_size;
    char hostname[256];
    char buf[1024];
    char path[1024];
    const char *p;
    av_log(NULL, AV_LOG_INFO, "rtp_open %s\n", uri);
    s = av_mallocz(sizeof(RTPContext));
    if (!s) {
        return AVERROR(ENOMEM);
    }
    h->priv_data = s;
    init_def_settings();

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &rtp_port,
                 path, sizeof(path), uri);
    /* extract parameters */
    ttl = -1;
    rtcp_port = rtp_port + 1;
    local_rtp_port = -1;
    local_rtcp_port = -1;
    max_packet_size = -1;
    connect = 0;

    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "ttl", p)) {
            ttl = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "rtcpport", p)) {
            rtcp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localport", p)) {
            local_rtp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localrtpport", p)) {
            local_rtp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "localrtcpport", p)) {
            local_rtcp_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "pkt_size", p)) {
            max_packet_size = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "connect", p)) {
            connect = strtol(buf, NULL, 10);
        }/*
        if (av_find_info_tag(buf, sizeof(buf), "use_cache", p)) {
            s->use_cache = strtol(buf, NULL, 10);
        }  */
    }
    s->use_cache = (flags & AVIO_FLAG_CACHE);

    build_udp_url(buf, sizeof(buf),
                  hostname, rtp_port, local_rtp_port, ttl, max_packet_size,
                  connect, 1);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]Setup udp session:%s\n", __FUNCTION__, __LINE__, buf);
    if (ffurl_open(&s->rtp_hd, buf, flags) < 0) {
        goto fail;
    }
    /* just to ease handle access. XXX: need to suppress direct handle
       access */
    s->rtp_fd = ffurl_get_file_handle(s->rtp_hd);

    if (!s->use_cache) {
        if (local_rtp_port >= 0 && local_rtcp_port < 0) {
            local_rtcp_port = ff_udp_get_local_port(s->rtp_hd) + 1;
        }

        build_udp_url(buf, sizeof(buf),
                      hostname, rtcp_port, local_rtcp_port, ttl, max_packet_size,
                      connect, 0);
        av_log(NULL, AV_LOG_INFO, "[%s:%d]Setup udp session:%s\n", __FUNCTION__, __LINE__, buf);
        if (ffurl_open(&s->rtcp_hd, buf, flags) < 0) {
            goto fail;
        }
        /* just to ease handle access. XXX: need to suppress direct handle
           access */
        s->rtcp_fd = ffurl_get_file_handle(s->rtcp_hd);
    }

    if (s->use_cache) {
        s->recvlist.max_items = 0;
        s->recvlist.item_ext_buf_size = 0;
        s->recvlist.muti_threads_access = 1;
        s->recvlist.reject_same_item_data = 0;
        itemlist_init(&s->recvlist) ;

        s->brunning = 1;
        av_log(NULL, AV_LOG_INFO, "[%s:%d]use cache mode\n", __FUNCTION__, __LINE__);
        if (amthreadpool_pthread_create_name(&(s->recv_thread), NULL, rtp_recv_task, s, "ffmpeg_rtp")) {
            av_log(NULL, AV_LOG_ERROR, "[%s:%d]ffmpeg_pthread_create failed\n", __FUNCTION__, __LINE__);
            goto fail;
        }
    }
    h->max_packet_size = s->rtp_hd->max_packet_size;
    h->is_streamed = 1;
    h->is_slowmedia = 1;

    return 0;

fail:
    if (s->rtp_hd) {
        ffurl_close(s->rtp_hd);
    }
    if (s->rtcp_hd) {
        ffurl_close(s->rtcp_hd);
    }
    av_free(s);
    return AVERROR(EIO);
}

static int check_net_phy_conn_status(void)
{
    int nNetDownOrUp = am_getconfig_int_def("net.ethwifi.up", 3); //0-eth&wifi both down, 1-eth up, 2-wifi up, 3-eth&wifi both up

    return nNetDownOrUp;
}

/*
FILE *g_dumpFile=NULL;
static void dumpFile(char *buf,int len){
    if (g_dumpFile == NULL)
        g_dumpFile=fopen("/data/tmp/rtp.ts","wb");

    if (g_dumpFile)
        fwrite(buf,1,len,g_dumpFile);
}

*/
static int rtp_close(URLContext *h);
static int rtp_read(URLContext *h, uint8_t *buf, int size)
{
    RTPContext *s = h->priv_data;
    int64_t starttime = ff_network_gettime();
    int64_t curtime;

    // Handle Network Down
    if (check_net_phy_conn_status() == 0) {
        // Network down
        if (s->network_down == 0) {
            s->network_down = 1;
            av_log(NULL, AV_LOG_INFO, "network down.\n");
        }
    } else if (check_net_phy_conn_status() > 0) {
        // Network up
        if (s->network_down == 1) {
            // reset rtp connection
            char *url = h->filename;
            int flags = h->flags | AVIO_FLAG_CACHE;
            rtp_close(h);
            rtp_open(h, url, flags);
            av_log(NULL, AV_LOG_INFO, "network up.rtp protocal reset finish.\n");
        }
    }

    if (s->network_down == 1) {
        return AVERROR(EAGAIN);
    }

    if (s->use_cache) {
        RTPPacket *lpkt = NULL;
        //uint8_t * lpkt_buf=NULL;
        //int len=0;
        int readsize = 0;
        int single_readsize = 0;
        while (s->brunning > 0 && size > readsize) {
            if (url_interrupt_cb()) {
                return AVERROR(EIO);
            }

            if (check_net_phy_conn_status() == 0) {
                break;
            }

            if (s->recvlist.item_count <= MIN_CACHE_PACKET_SIZE) {
                curtime = ff_network_gettime();
                if (starttime <= 0) {
                    starttime = curtime;
                }
                if (gd_report_error_enable && curtime > starttime + 30 * 1000 * 1000 && !s->report_flag) {
                    s->report_flag = 1;
                    ffmpeg_notify(h, PLAYER_EVENTS_ERROR, 54000, 0);
                }
                amthreadpool_thread_usleep(10);
                continue;
            }

            if (itemlist_peek_head_data(&(s->recvlist), (unsigned long *)&lpkt) != 0 || lpkt == NULL) {
                amthreadpool_thread_usleep(10);
                continue;
            }
            starttime = 0;
            s->report_flag = 0;

            single_readsize = min(lpkt->len - lpkt->valid_data_offset, size - readsize);
            memcpy(buf + readsize, lpkt->buf + lpkt->valid_data_offset, single_readsize);

            readsize += single_readsize;
            lpkt->valid_data_offset += single_readsize;
            if (lpkt->valid_data_offset >= lpkt->len) {
                if ((s->last_seq + 1) % MAX_RTP_SEQ != lpkt->seq) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d]discontinuity seq=%d, the right seq=%d\n", __FUNCTION__, __LINE__, lpkt->seq, (s->last_seq + 1) % MAX_RTP_SEQ);
                }
                s->last_seq = lpkt->seq;

                // already read, no valid data clean it
                itemlist_del_match_data_item(&(s->recvlist), (unsigned long)lpkt);
                rtp_free_packet((void *)lpkt);
                lpkt = NULL;
            }
        }

        return readsize;
    } else {
        return inner_rtp_read(s, buf, size, h);
    }
}

static int rtp_write(URLContext *h, const uint8_t *buf, int size)
{
    RTPContext *s = h->priv_data;
    int ret;
    URLContext *hd;

    if (buf[1] >= RTCP_SR && buf[1] <= RTCP_APP) {
        /* RTCP payload type */
        hd = s->rtcp_hd;
    } else {
        /* RTP payload type */
        hd = s->rtp_hd;
    }

    ret = ffurl_write(hd, buf, size);
#if 0
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10 * 1000000;
        nanosleep(&ts, NULL);
    }
#endif
    return ret;
}

static int rtp_close(URLContext *h)
{
    RTPContext *s = h->priv_data;

    if (s->use_cache) {
        s->brunning = 0;
        amthreadpool_pthread_join(s->recv_thread, NULL);
        s->recv_thread = 0;

        itemlist_clean(&s->recvlist, rtp_free_packet);
    }

    if (s->rtp_hd) {
        ffurl_close(s->rtp_hd);
    }
    if (s->rtcp_hd) {
        ffurl_close(s->rtcp_hd);
    }
    av_free(s);
    return 0;
}

/**
 * Return the local rtp port used by the RTP connection
 * @param h media file context
 * @return the local port number
 */

int rtp_get_local_rtp_port(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return ff_udp_get_local_port(s->rtp_hd);
}

/**
 * Return the local rtcp port used by the RTP connection
 * @param h media file context
 * @return the local port number
 */

int rtp_get_local_rtcp_port(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return ff_udp_get_local_port(s->rtcp_hd);
}

static int rtp_get_file_handle(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return s->rtp_fd;
}

int rtp_get_rtcp_file_handle(URLContext *h)
{
    RTPContext *s = h->priv_data;
    return s->rtcp_fd;
}

// ---------------------------------------------------------------------
// rtpfec protocol

static int rtpfec_free_packet(void * apkt)
{
    RTPFECPacket * lpkt = apkt;
    if (lpkt != NULL) {
        if (lpkt->buf != NULL) {
            av_free(lpkt->buf);
        }
        if (lpkt->fec != NULL) {
            av_free(lpkt->fec);
        }
        av_free(lpkt);
    }
    apkt = NULL;
    return 0;
}

static int rtpfec_read_data(RTPFECContext * s, uint8_t *buf, int size)
{
    struct sockaddr_storage from;
    socklen_t from_len;
    int len, n;
    struct pollfd p[2] = {{s->rtp_fd, POLLIN, 0}, {s->fec_fd, POLLIN, 0}};

    for (;;) {
        if (url_interrupt_cb()) {
            return AVERROR_EXIT;
        }

        /* build fdset to listen to RTP and fec packets */
        n = poll(p, 2, 100);
        if (n > 0) {
            /* first try FEC */
            if (p[1].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom(s->fec_fd, buf, size, 0,
                               (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR)) {
                        TRACE()
                        usleep(10);
                        continue;
                    }
                    return AVERROR(EIO);
                }
                break;
            }

            /* then RTP */
            if (p[0].revents & POLLIN) {
                from_len = sizeof(from);
                len = recvfrom(s->rtp_fd, buf, size, 0,
                               (struct sockaddr *)&from, &from_len);
                if (len < 0) {
                    if (ff_neterrno() == AVERROR(EAGAIN) ||
                        ff_neterrno() == AVERROR(EINTR)) {
                        TRACE()
                        usleep(10);
                        continue;
                    }
                    return AVERROR(EIO);
                }

                break;
            }
            TRACE()
        } else if (n < 0) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]network error n=%d\n", __FUNCTION__, __LINE__, n);
            if (ff_neterrno() == AVERROR(EINTR)) {
                usleep(10);
                continue;
            }
            return AVERROR(EIO);
        }

        TRACE()
        usleep(10);
    }

    return len;
}

static int rtpfec_enqueue_packet(struct itemlist *itemlist, RTPFECPacket * lpkt)
{
    int ret = 0;
    TRACE()
    RTPFECPacket *ltailpkt = NULL;
    itemlist_peek_tail_data(itemlist, (unsigned long*)&ltailpkt) ;
    if (NULL == ltailpkt || (ltailpkt != NULL && seq_less_and_equal(ltailpkt->seq, lpkt->seq) == 1)) {
        // append to the tail
        TRACE()
        ret = itemlist_add_tail_data(itemlist, (unsigned long)lpkt) ;
    } else {
        // insert to the queue
        struct item *item = NULL;
        struct item *newitem = NULL;
        struct list_head *llist = NULL, *tmplist = NULL;
        RTPFECPacket *llistpkt = NULL;

        TRACE()
        ITEM_LOCK(itemlist);
        if (itemlist->max_items > 0 && itemlist->max_items <= itemlist->item_count) {
            ITEM_UNLOCK(itemlist);
            return -1;
        }

        list_for_each_safe(llist, tmplist, &itemlist->list) {
            item = list_entry(llist, struct item, list);
            llistpkt = (RTPFECPacket *)(item->item_data);
            if (seq_less(lpkt->seq, llistpkt->seq) == 1) {
                // insert to front
                newitem = item_alloc(itemlist->item_ext_buf_size);
                if (newitem == NULL) {
                    ITEM_UNLOCK(itemlist);
                    return -12;//noMEM
                }
                newitem->item_data = (unsigned long)lpkt;

                list_add_tail(&(newitem->list), &(item->list));
                itemlist->item_count++;
                break;
            }
        }
        ITEM_UNLOCK(itemlist);
    }
    TRACE()
    return ret;
}

static int rtpfec_enqueue_outpacket(RTPFECContext * s, RTPFECPacket * lpkt)
{
    int try_cnt = 1;
    int ret = itemlist_add_tail_data(&(s->outlist), (unsigned long)lpkt) ;
    while (ret < 0) {   // keyinfo try 6
        if (url_interrupt_cb()) {
            rtpfec_free_packet(lpkt);
            return -1;
        }
        amthreadpool_thread_usleep(10);

        // retry
        ret = itemlist_add_tail_data(&(s->outlist), (unsigned long)lpkt) ;
        try_cnt++;
    }

    return ret;
}

static void do_decode_output(RTPFECContext * s)
{
    // decode
    RTPFECPacket *lpkt = NULL;
    if (s->fec_handle == NULL || s->cur_fec == NULL) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d]fec_handle=%x cur_fec=%x\n", __FUNCTION__, __LINE__, s->fec_handle, s->cur_fec);
        return;
    }

    memset(rtp_data_array, 0, sizeof(rtp_data_array));
    memset(rtp_packet, 0, sizeof(rtp_packet));
    memset(fec_data_array, 0, sizeof(fec_data_array));
    memset(fec_packet, 0, sizeof(fec_packet));
    memset(lost_map, 0, sizeof(lost_map));

    int blose_packet = 0;
    int has_packet = 0;
    int index = 0;
    int i = 0;
    int rtp_valid_cnt = 0;

    // put the fec packet of same group to decoder vector
    lpkt = NULL;
    while (itemlist_peek_head_data(&(s->feclist), (unsigned long*)&lpkt) == 0 && lpkt != NULL &&
           s->cur_fec->rtp_begin_seq == lpkt->fec->rtp_begin_seq && s->cur_fec->rtp_end_seq == lpkt->fec->rtp_end_seq) {
        itemlist_get_head_data(&(s->feclist), (unsigned long*)&lpkt) ;
        if (lpkt != NULL && lpkt->fec->redund_idx < MAX_FEC_PACKET_NUM) {
            fec_packet[lpkt->fec->redund_idx] = lpkt;
            fec_data_array[lpkt->fec->redund_idx] = lpkt->fec->fec_data;
            lost_map[s->rtp_media_packet_sum + lpkt->fec->redund_idx] = 1;
            //av_log(NULL, AV_LOG_INFO, "[%s:%d]put to fec decoder vector. idx=%d\n", __FUNCTION__, __LINE__,lpkt->fec->redund_idx);
        } else if (lpkt != NULL) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]fec out of boundary. idx=%d\n", __FUNCTION__, __LINE__, lpkt->fec->redund_idx);
            rtpfec_free_packet((void *)lpkt);
        }
        lpkt = NULL;
    }
    // put the rtp packet of same group to decoder vector
    lpkt = NULL;
    while (itemlist_peek_head_data(&s->recvlist, (unsigned long*)&lpkt) == 0 && lpkt != NULL &&
           seq_less_and_equal(s->cur_fec->rtp_begin_seq, lpkt->seq) == 1 && seq_less_and_equal(lpkt->seq, s->cur_fec->rtp_end_seq) == 1) {
        has_packet = 1;
        itemlist_get_head_data(&s->recvlist, (unsigned long*)&lpkt);
        //if(lpkt != NULL&&lpkt->seq<=s->cur_fec->rtp_end_seq-3){
        if (lpkt != NULL) {
            index = seq_subtraction(lpkt->seq, s->cur_fec->rtp_begin_seq);
            if (0 <= index && index < s->rtp_media_packet_sum) {
                rtp_packet[index] = lpkt;
                rtp_data_array[index] = lpkt->buf;
                lost_map[index] = 1;
                rtp_valid_cnt++;
                //av_log(NULL, AV_LOG_INFO, "[%s:%d]input rtp data idx=%d,seq=%d\n", __FUNCTION__, __LINE__,index,lpkt->seq);
            } else {
                av_log(NULL, AV_LOG_INFO, "[%s:%d]rtp out of boundary. idx=%d\n", __FUNCTION__, __LINE__, index);
                rtpfec_free_packet((void *)lpkt);
            }
        }/*
    else if(lpkt != NULL){
        index=lpkt->seq - s->cur_fec->rtp_begin_seq;
        av_log(NULL, AV_LOG_INFO, "[%s:%d]to discard . idx=%d,seq=%d\n", __FUNCTION__, __LINE__,index,lpkt->seq);
        rtpfec_free_packet((void *)lpkt);
    }*/
        lpkt = NULL;
    }
    //av_log(NULL, AV_LOG_INFO, "[%s:%d]rtp_media_sum=%d,rtp_valid_cnt=%d\n", __FUNCTION__, __LINE__,s->rtp_media_packet_sum,rtp_valid_cnt);

    if ((s->rtp_media_packet_sum - rtp_valid_cnt) > s->cur_fec->redund_num) {
        int direct_output_cnt = 0;
        for (i = 0; i < s->rtp_media_packet_sum; i++) {
            if (lost_map[i] == 1) {
                rtpfec_enqueue_outpacket(s, rtp_packet[i]);
                direct_output_cnt++;
            }
        }
        av_log(NULL, AV_LOG_INFO, "[%s:%d]To lose too much,directly output.output_cnt=%d\n", __FUNCTION__, __LINE__, direct_output_cnt);
        goto QUIT_DECODE;
    }

    if (has_packet) {

        // malloc the lose packet to the fec decoder vector
        lpkt = NULL;
        for (i = 0; i < s->cur_fec->redund_num; i++) {
            if (lost_map[s->rtp_media_packet_sum + i] == 0) {
                lpkt = av_mallocz(sizeof(RTPFECPacket));
                if (lpkt == NULL) {
                    av_log(NULL, AV_LOG_INFO, "[%s:%d]lpkt == NULL\n", __FUNCTION__, __LINE__);
                    continue;
                }

                lpkt->buf = av_malloc(FEC_RECVBUF_SIZE);
                lpkt->len = s->cur_fec->fec_len;
                fec_packet[i] = lpkt;
                fec_data_array[i] = lpkt->buf;
                lpkt->fec = NULL;
                av_log(NULL, AV_LOG_INFO, "[%s:%d]lose fec packet,index=%d,lost_map index=%d\n", __FUNCTION__, __LINE__, i, s->rtp_media_packet_sum + i);
            }
        }

        // malloc the lose packet to the rtp decoder vector
        lpkt = NULL;
        for (i = 0; i < s->rtp_media_packet_sum; i++) {
            if (lost_map[i] == 0) {
                blose_packet = 1;
                lpkt = av_mallocz(sizeof(RTPFECPacket));
                if (lpkt == NULL) {
                    av_log(NULL, AV_LOG_INFO, "[%s:%d]lpkt == NULL\n", __FUNCTION__, __LINE__);
                    continue;
                }

                lpkt->buf = av_malloc(FEC_RECVBUF_SIZE);
                lpkt->len = s->cur_fec->rtp_len;
                lpkt->seq = (s->cur_fec->rtp_begin_seq + i) % MAX_RTP_SEQ;
                rtp_packet[i] = lpkt;
                rtp_data_array[i] = lpkt->buf;
                av_log(NULL, AV_LOG_INFO, "[%s:%d]lose rtp packet,lost_map index=%d,req=%d\n", __FUNCTION__, __LINE__, i, lpkt->seq);
            }
            lpkt = NULL;
        }

        // decoder the packet
        if (blose_packet == 1 && s->fec_handle != NULL) {
            //av_log(NULL, AV_LOG_INFO, "[%s:%d]lose rtp packet, do decode i0=%d i1=%d,lostaddr=%x\n", __FUNCTION__, __LINE__,lost_map[0],lost_map[1],lost_map);
            int ret = fec_decode(s->fec_handle, rtp_data_array, fec_data_array, lost_map, s->cur_fec->rtp_len);
            if (ret != 0) {
                for (i = 0; i < s->rtp_media_packet_sum; i++) {
                    if (lost_map[i] == 1) {
                        rtpfec_enqueue_outpacket(s, rtp_packet[i]);
                    } else {
                        if (rtp_packet[i] != NULL) {
                            rtpfec_free_packet((void *)(rtp_packet[i]));
                        }
                        rtp_packet[i] = NULL;
                        rtp_data_array[i] = NULL;
                    }
                }
                av_log(NULL, AV_LOG_INFO, "[%s:%d]decode failed ret=%d, to output the valide data\n", __FUNCTION__, __LINE__, ret);
                goto QUIT_DECODE;
            } else {
                av_log(NULL, AV_LOG_INFO, "[%s:%d]decode success ret=%d\n", __FUNCTION__, __LINE__, ret);
            }
        }

        // all output
        for (i = 0; i < s->rtp_media_packet_sum; i++) {
            rtpfec_enqueue_outpacket(s, rtp_packet[i]);
        }

        //av_log(NULL, AV_LOG_INFO, "[%s:%d]output packet num=%d\n", __FUNCTION__, __LINE__,s->rtp_media_packet_sum);
    }

QUIT_DECODE:
    s->rtp_last_decode_seq = s->cur_fec->rtp_end_seq;
    for (int i = 0; i < s->cur_fec->redund_num && i < MAX_FEC_PACKET_NUM; i++) {
        if (fec_packet[i] != NULL) {
            rtpfec_free_packet((void *)(fec_packet[i]));
        }
    }
    s->cur_fec = NULL;
    //av_log(NULL, AV_LOG_INFO, "[%s:%d]reset_fecdata last_decode_seq=%d\n", __FUNCTION__, __LINE__,s->rtp_last_decode_seq);
}

static void rtpfec_output_packet(RTPFECContext * s)
{
    RTPFECPacket *lheadpkt = NULL;
    RTPFECPacket *lpkt = NULL;

    if (s->bdecode) {
        TRACE()
        if (s->cur_fec == NULL) {
            // to check the fec array is full, to set the cur_fec
            itemlist_peek_head_data(&(s->feclist), (unsigned long*)&lheadpkt) ;
            if (lheadpkt != NULL && s->feclist.item_count > lheadpkt->fec->redund_num) {
                int rtp_begin_seq = lheadpkt->fec->rtp_begin_seq;
                int rtp_end_seq = lheadpkt->fec->rtp_end_seq;
                s->cur_fec = lheadpkt->fec;

                if (s->fec_handle == NULL) {
                    s->rtp_media_packet_sum = seq_subtraction(rtp_end_seq, rtp_begin_seq) + 1;
                    init_RS_fec();
                    s->fec_handle = RS_fec_new(s->rtp_media_packet_sum, s->cur_fec->redund_num);
                }

                av_log(NULL, AV_LOG_INFO, "[%s:%d]req=%d,rtp_sum=%d,redund_num=%d,begin=%d,end=%d,rtp_len=%d\n", __FUNCTION__, __LINE__,
                       lheadpkt->seq, s->rtp_media_packet_sum, lheadpkt->fec->redund_num, rtp_begin_seq, rtp_end_seq, lheadpkt->fec->rtp_len);
            }
        }
        TRACE()
        if (s->cur_fec != NULL) {
            // output the forward packet directly
            lpkt = NULL;
            while (itemlist_peek_head_data(&(s->recvlist), (unsigned long*)&lpkt) == 0 && lpkt != NULL && seq_less(lpkt->seq, s->cur_fec->rtp_begin_seq)) {
                itemlist_get_head_data(&s->recvlist, (unsigned long*)&lpkt) ;
                if (lpkt != NULL) {
                    rtpfec_enqueue_outpacket(s, lpkt);
                }
                lpkt = NULL;
            }

            //int reset_fecdata=0;
            if (s->recvlist.item_count > s->rtp_media_packet_sum + EXTRA_BUFFER_PACKET_NUM) {
                // if the receive buffer enough packet , to do decode and output
                //av_log(NULL, AV_LOG_INFO, "[%s:%d]do_decode_output item count=%d,sum\n", __FUNCTION__, __LINE__,s->recvlist.item_count,s->rtp_media_packet_sum);
                int fec_enough = (int)(s->feclist.item_count >= s->cur_fec->redund_num);
                int rtp_enough = (int)(s->recvlist.item_count >= s->rtp_media_packet_sum);
                if (fec_enough && rtp_enough) {
                    do_decode_output(s);
                }
                //reset_fecdata=1;
            }
        }
        TRACE()
    }

    if (s->recvlist.item_count > FORCE_OUTPUT_PACKET_NUM_THRESHOLD && s->cur_fec == NULL) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d]direct to output the packet item_count=%d num=%d\n", __FUNCTION__, __LINE__, s->recvlist.item_count, FORCE_OUTPUT_PACKET_NUM);
        // force to output
        lpkt = NULL;
        while (s->recvlist.item_count > 0) {
            itemlist_get_head_data(&s->recvlist, (unsigned long*)&lpkt);
            if (lpkt != NULL) {
                rtpfec_enqueue_outpacket(s, lpkt);
            }
            lpkt = NULL;
        }
    }
}

static void rtpfec_reset_packet(RTPFECPacket *lpkt)
{
    if (lpkt == NULL) {
        return;
    }

    lpkt->seq = 0;
    lpkt->len = 0;
    lpkt->payload_type = 0;

    if (lpkt->buf != NULL) {
        memset(lpkt->buf , 0, FEC_RECVBUF_SIZE);
    }

    if (lpkt->fec != NULL) {
        lpkt->fec->rtp_begin_seq = 0;
        lpkt->fec->rtp_end_seq = 0;
        lpkt->fec->redund_num = 0;
        lpkt->fec->redund_idx = 0;
        lpkt->fec->fec_len = 0;
        lpkt->fec->rtp_len = 0;
        lpkt->fec->rsv = 0;
        lpkt->fec->fec_data = NULL;
    }
}

static void *rtpfec_recv_task(void *_RTPFECContext)
{
    av_log(NULL, AV_LOG_INFO, "[%s:%d]recv_buffer_task start running!!!\n", __FUNCTION__, __LINE__);
    RTPFECContext * s = (RTPFECContext *)_RTPFECContext;
    if (NULL == s) {
        av_log(NULL, AV_LOG_INFO, "[%s:%d]Null handle!!!\n", __FUNCTION__, __LINE__);
        goto thread_end;
    }

    RTPFECPacket * lpkt = NULL;
    int datalen = 0, ext ;
    uint8_t * lpoffset;
    int ret = 0;
    int try_cnt = 0;

    while (s->brunning > 0) {
        if (url_interrupt_cb()) {
            goto thread_end;
        }
        // malloc the packet buffer
        if (NULL == lpkt) {
            lpkt = av_mallocz(sizeof(RTPFECPacket));
            if (NULL == lpkt) {
                goto thread_end;
            }
        } else {
            lpkt->len = 0;
            lpkt->payload_type = 0;
            lpkt->seq = 0;
        }
        if (NULL == lpkt->buf) {
            lpkt->buf = av_malloc(FEC_RECVBUF_SIZE);
            if (NULL == lpkt->buf) {
                goto thread_end;
            }
            memset(lpkt->buf, 0, FEC_RECVBUF_SIZE);
        } else {
            memset(lpkt->buf, 0, FEC_RECVBUF_SIZE);
        }

        TRACE()

        // recv data
        lpkt->len = rtpfec_read_data(s, lpkt->buf, FEC_RECVBUF_SIZE);
        if (lpkt->len <= 12) {
            av_log(NULL, AV_LOG_INFO, "[%s:%d]receive wrong packet len=%d \n", __FUNCTION__, __LINE__, lpkt->len);
            usleep(10);
            continue;
        }
        TRACE()

        // paser data and buffer the packat
        lpkt->payload_type = lpkt->buf[1] & 0x7f;
        lpkt->seq = AV_RB16(lpkt->buf + 2);
        if (lpkt->payload_type == FEC_PAYLOAD_TYPE) {        // fec packet
            //av_log(NULL, AV_LOG_INFO, "[%s:%d]datalen=%d\n", __FUNCTION__, __LINE__,lpkt->len);
            // parse the fec header
            datalen = lpkt->len ;
            if (lpkt->buf[0] & 0x20) {           // remove the padding padding (P): 1 bit
                int padding = lpkt->buf[datalen - 1];
                if (datalen >= 12 + padding) {
                    datalen -= padding;
                }
                av_log(NULL, AV_LOG_INFO, "[%s:%d]padding=%d\n", __FUNCTION__, __LINE__, padding);
            }

            datalen -= 12;                      // The first twelve octets are present in every RTP packet
            lpoffset = lpkt->buf + 12;

            // RFC 3550 Section 5.3.1 RTP Header Extension handling
            ext = lpkt->buf[0] & 0x10;
            if (ext > 0) {
                TRACE()
                if (datalen < 4) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d]datalen<4\n", __FUNCTION__, __LINE__);
                    continue;
                }
                ext = (AV_RB16(lpoffset + 2) + 1) << 2;
                if (datalen < ext) {
                    av_log(NULL, AV_LOG_ERROR, "[%s:%d]ext = %d\n", __FUNCTION__, __LINE__, ext);
                    continue;
                }

                datalen -= ext ;
                lpoffset += ext ;
                av_log(NULL, AV_LOG_INFO, "[%s:%d]ext=%d\n", __FUNCTION__, __LINE__, ext);
            }

            if (NULL == lpkt->fec) {
                lpkt->fec = av_mallocz(sizeof(FEC_DATA_STRUCT));
                if (NULL == lpkt->fec) {
                    goto thread_end;
                }
            } else {
                memset(lpkt->fec, 0, sizeof(FEC_DATA_STRUCT));
            }

            lpkt->fec->rtp_begin_seq = AV_RB16(lpoffset);
            lpkt->fec->rtp_end_seq = AV_RB16(lpoffset + 2);
            lpkt->fec->redund_num = *(lpoffset + 4);
            lpkt->fec->redund_idx = *(lpoffset + 5);
            lpkt->fec->fec_len = AV_RB16(lpoffset + 6);
            lpkt->fec->rtp_len = AV_RB16(lpoffset + 8);
            lpkt->fec->fec_data = lpoffset + 12;

            av_log(NULL, AV_LOG_ERROR, "[%s:%d]seq=%d,rtp_begin_seq=%d,rtp_end_seq=%d,redund_num=%d,redund_idx=%d,rtp_len=%d\n", __FUNCTION__, __LINE__,
                   lpkt->seq, lpkt->fec->rtp_begin_seq, lpkt->fec->rtp_end_seq, lpkt->fec->redund_num, lpkt->fec->redund_idx, lpkt->fec->rtp_len);

            try_cnt = 1;
            ret = rtpfec_enqueue_packet(&(s->feclist), lpkt);
            while (ret < 0 && try_cnt <= 6) { // keyinfo try 6
                if (url_interrupt_cb()) {
                    goto thread_end;
                }
                amthreadpool_thread_usleep(10);

                // retry
                ret = rtpfec_enqueue_packet(&(s->feclist), lpkt);
                try_cnt++;
            }

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d]feclist have no room. timeout\n", __FUNCTION__, __LINE__);
                continue;
            }
        } else if (lpkt->payload_type == 33) {  // mpegts packet
            //av_log(NULL, AV_LOG_ERROR, "[%s:%d]mpegts packet req = %d\n", __FUNCTION__, __LINE__, lpkt->seq);
            try_cnt = 1;
            ret = rtpfec_enqueue_packet(&(s->recvlist), lpkt);
            while (ret < 0 && try_cnt <= 3) { // try 3
                if (url_interrupt_cb()) {
                    goto thread_end;
                }
                amthreadpool_thread_usleep(10);

                // retry
                ret = rtpfec_enqueue_packet(&(s->recvlist), lpkt);
                try_cnt++;
            }

            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d]recvlist have no room. timeout\n", __FUNCTION__, __LINE__);
                continue;
            }
        } else {
            av_log(NULL, AV_LOG_ERROR, "[%s:%d]unknow payload type = %d, seq=%d\n", __FUNCTION__, __LINE__, lpkt->payload_type, lpkt->seq);
            continue;
        }

        TRACE()
        rtpfec_output_packet(s);
        TRACE()
        lpkt = NULL;
    }

thread_end:
    s->brunning = 0;
    rtpfec_free_packet((void *)lpkt);
    av_log(NULL, AV_LOG_ERROR, "[%s:%d]recv_buffer_task end!!!\n", __FUNCTION__, __LINE__);
    return NULL;
}

static int rtpfec_open(URLContext *h, const char *uri, int flags)
{
    RTPFECContext *s;
    int rtp_port = -1;
    int fec_port = -1;
    int connect = 0;
    int ttl = -1;
    int local_rtp_port = -1;
    int max_packet_size = -1;
    char hostname[256] = {0};
    char buf[1024] = {0};
    char path[1024] = {0};
    const char *p;

    s = av_mallocz(sizeof(RTPFECContext));
    av_log(NULL, AV_LOG_INFO, "[%s:%d]s= %x\n", __FUNCTION__, __LINE__, s);
    if (!s) {
        return AVERROR(ENOMEM);
    }
    h->priv_data = s;

    av_url_split(NULL, 0, NULL, 0, hostname, sizeof(hostname), &rtp_port,
                 path, sizeof(path), uri);

    p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "ChannelFECPort", p)) {
            fec_port = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "pkt_size", p)) {
            max_packet_size = strtol(buf, NULL, 10);
        }
        if (av_find_info_tag(buf, sizeof(buf), "connect", p)) {
            connect = strtol(buf, NULL, 10);
        }
    }

    if (fec_port < 0) {
        goto fail;
    }

    build_udp_url(buf, sizeof(buf),
                  hostname, rtp_port, local_rtp_port, ttl, max_packet_size,
                  connect, 1);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]build rtp udp url %s\n", __FUNCTION__, __LINE__, buf);
    if (ffurl_open(&s->rtp_hd, buf, flags) < 0) {
        goto fail;
    }

    memset(buf, 0, sizeof(buf));
    build_udp_url(buf, sizeof(buf),
                  hostname, fec_port, -1, ttl, max_packet_size,
                  connect, 0);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]build fec udp url %s\n", __FUNCTION__, __LINE__, buf);
    if (ffurl_open(&s->fec_hd, buf, flags) < 0) {
        goto fail;
    }

    /* just to ease handle access. XXX: need to suppress direct handle
       access */
    s->rtp_fd = ffurl_get_file_handle(s->rtp_hd);
    s->fec_fd = ffurl_get_file_handle(s->fec_hd);

    s->recvlist.max_items = 2000;
    s->recvlist.item_ext_buf_size = 0;
    s->recvlist.muti_threads_access = 1;
    s->recvlist.reject_same_item_data = 0;
    itemlist_init(&s->recvlist) ;

    s->outlist.max_items = 2000;
    s->outlist.item_ext_buf_size = 0;
    s->outlist.muti_threads_access = 1;
    s->outlist.reject_same_item_data = 0;
    itemlist_init(&s->outlist) ;

    s->feclist.max_items = 500;
    s->feclist.item_ext_buf_size = 0;
    s->feclist.muti_threads_access = 1;
    s->feclist.reject_same_item_data = 0;
    itemlist_init(&s->feclist) ;

    s->rtp_seq_discontinue = 0;
    s->fec_seq_discontinue = 0;
    s->cur_fec = NULL;
    s->bdecode = 1;     // 0:test 1:decode
    s->brunning = 1;
    av_log(NULL, AV_LOG_INFO, "[%s:%d]s= %x,bdecode=%d,brunning=%d\n", __FUNCTION__, __LINE__, s, s->bdecode, s->brunning);
    if (amthreadpool_pthread_create(&(s->recv_thread), NULL, rtpfec_recv_task, s)) {
        av_log(NULL, AV_LOG_ERROR, "[%s:%d]ffmpeg_pthread_create failed\n", __FUNCTION__, __LINE__);
        goto fail;
    }

    h->max_packet_size = s->rtp_hd->max_packet_size;
    h->is_streamed = 1;
    return 0;

fail:
    if (s->rtp_hd) {
        ffurl_close(s->rtp_hd);
    }
    if (s->fec_hd) {
        ffurl_close(s->fec_hd);
    }
    av_free(s);
    return AVERROR(EIO);
}

static int rtpfec_read(URLContext *h, uint8_t *buf, int size)
{
    RTPFECContext *s = h->priv_data;
    if (s == NULL) {
        return AVERROR(EIO);
    }

    RTPFECPacket *lpkt = NULL;
    uint8_t * lpkt_buf = NULL;
    int len = 0;
    while (s->brunning > 0) {
        if (url_interrupt_cb()) {
            return AVERROR(EIO);
        }

        if (itemlist_get_head_data(&s->outlist, (unsigned long*)&lpkt) != 0 && lpkt == NULL) {
            usleep(30);
            continue;
        }

        lpkt_buf = lpkt->buf;
        len = lpkt->len;

        if (lpkt_buf[0] & 0x20) {                   // remove the padding data
            int padding = lpkt_buf[len - 1];
            if (len >= 12 + padding) {
                len -= padding;
            }
        }

        if (len <= 12) {
            av_log(NULL, AV_LOG_ERROR, "[%s:%d]len<=12,len=%d\n", __FUNCTION__, __LINE__, len);
            goto read_continue;
        }

        // output the playload data
        int offset = 12 ;
        uint8_t * lpoffset = lpkt_buf + 12;

        int ext = lpkt_buf[0] & 0x10;
        if (ext > 0) {
            TRACE()
            if (len < offset + 4) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < offset + 4\n", __FUNCTION__, __LINE__);
                goto read_continue;
            }

            ext = (AV_RB16(lpoffset + 2) + 1) << 2;
            if (len < ext + offset) {
                av_log(NULL, AV_LOG_ERROR, "[%s:%d]len < ext + offset\n", __FUNCTION__, __LINE__);
                goto read_continue;
            }
            offset += ext ;
            lpoffset += ext ;
        }

        memcpy(buf, lpoffset, len - offset) ;
        len -= offset ;
        break;

read_continue:
        rtpfec_free_packet((void *)lpkt);
        lpkt = NULL;
        lpkt_buf = NULL;
        len = 0;
    }

    rtpfec_free_packet((void *)lpkt);
    return len;
}

static int rtpfec_close(URLContext *h)
{
    RTPFECContext *s = h->priv_data;

    s->brunning = 0;
    amthreadpool_pthread_join(s->recv_thread, NULL);
    s->recv_thread = 0;

    av_log(NULL, AV_LOG_INFO, "[%s:%d]cur_fec=0x%x,media_packet_sum=%d\n", __FUNCTION__, __LINE__, s->cur_fec, s->rtp_media_packet_sum);

    s->cur_fec = NULL;
    av_log(NULL, AV_LOG_INFO, "[%s:%d]recvlist item_count=%d,max_items=%d\n", __FUNCTION__, __LINE__, s->recvlist.item_count, s->recvlist.max_items);
    itemlist_clean(&s->recvlist, rtpfec_free_packet);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]outlist item_count=%d,max_items=%d\n", __FUNCTION__, __LINE__, s->recvlist.item_count, s->recvlist.max_items);
    itemlist_clean(&s->outlist, rtpfec_free_packet);
    av_log(NULL, AV_LOG_INFO, "[%s:%d]feclist item_count=%d,max_items=%d\n", __FUNCTION__, __LINE__, s->recvlist.item_count, s->recvlist.max_items);
    itemlist_clean(&s->feclist, rtpfec_free_packet);

    if (s->fec_handle == NULL) {
        RS_fec_free(s->fec_handle);
    }
    s->fec_handle = NULL;

    ffurl_close(s->rtp_hd);
    ffurl_close(s->fec_hd);
    av_free(s);
    return 0;
}

URLProtocol ff_rtp_protocol = {
    .name                = "rtp",
    .url_open            = rtp_open,
    .url_read            = rtp_read,
    .url_write           = rtp_write,
    .url_close           = rtp_close,
    .url_get_file_handle = rtp_get_file_handle,
};

URLProtocol ff_rtpfec_protocol = {
    .name                = "rtpfec",
    .url_open            = rtpfec_open,
    .url_read            = rtpfec_read,
    .url_write           = NULL,
    .url_close           = rtpfec_close,
};
