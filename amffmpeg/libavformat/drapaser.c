/*
 * dra demuxer

 */


#include "libavutil/crc.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/log.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavcodec/bytestream.h"
#include "avformat.h"
#include "internal.h"
#include "avio_internal.h"
#include "seek.h"
#include "isom.h"

#include <amthreadpool.h>
#include <dlfcn.h>
#include <stdio.h>
#include "drapaser.h"
#include "rawdec.h"


struct DraContext {
    const AVClass *class;
    /* user data */
    AVFormatContext *stream;
    int raw_packet_size;
    int first_packet;

    int pos47;

    /* data needed to handle file based ts */
    /** stop parsing loop                                    */
    int stop_parse;
    /** packet containing Audio/Video data                   */
    AVPacket *pkt;
    /** to detect seek                                       */
    int64_t last_pos;

    int64_t first_pcrscr;
};

typedef struct {
    uint32_t stream_type;
    enum AVMediaType codec_type;
    enum CodecID codec_id;
} StreamType;

static int getframelength(char* inbuf)
{
    int i;
    long bit4frmlen = 0;
    int frmlen = 0;
    unsigned short temp;
    memcpy(&temp, inbuf + 2, 2);
#if 1
    temp = (temp >> 8) | (temp << 8);
#endif
    bit4frmlen = ((temp & 0x8000) == 0) ? 10 : 13;
    frmlen = temp << 1;
    frmlen = (frmlen >> (16 - bit4frmlen)) * 4;
    if (frmlen > 4096) {
        return 0;
    }
    return frmlen;
}


static int dra_probe(AVProbeData *p)
{
    const int size = p->buf_size;
    char *ppbuf = p->buf;
    int framelength = getframelength(ppbuf);
    if (ppbuf[0] == 0x7f && ppbuf[1] == 0xff && ppbuf[framelength] == 0x7f && ppbuf[framelength + 1] == 0xff) {
        return AVPROBE_SCORE_MAX;
    }
    return -1;

}

static int dra_read_header(AVFormatContext *s,
                           AVFormatParameters *ap)
{
    AVStream *st;
    int err;
    uint8_t *buf = s->pb->buffer;
    st = av_new_stream(s, 0);
    if (!st) {
        return AVERROR(ENOMEM);
    }

    st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
    st->codec->codec_id = s->iformat->value;
    st->need_parsing = AVSTREAM_PARSE_FULL;
    return 0;

}

static int dra_read_packet(AVFormatContext *s,
                           AVPacket *pkt)
{

    int ret;
    //avio_read(pb, buf_read, pkt_size);
    return 0;
}

static int dra_read_close(AVFormatContext *s)
{
    DraContext *ts = s->priv_data;
    return 0;
}


AVInputFormat ff_dra_demuxer = {
    "dra",
    NULL_IF_CONFIG_SMALL("raw dra"),
    0,
    dra_probe,
    dra_read_header,
    ff_raw_read_partial_packet,
    dra_read_close,
    .flags = AVFMT_GENERIC_INDEX,
    .extensions = "dra",
    .value = CODEC_ID_DRA,
};
