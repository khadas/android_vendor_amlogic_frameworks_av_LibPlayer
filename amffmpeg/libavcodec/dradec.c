
/*fake dradec module  for ffmpeg demux*/

#include "avcodec.h"
#include "get_bits.h"
#include "bytestream.h"
#include "unary.h"
#include "mathops.h"

#define MAX_CHANNELS 2

typedef struct {

    AVCodecContext *avctx;
    GetBitContext gb;

    int numchannels;
    int bytespersample;

} DRAContext;

static int dra_set_info(DRAContext *dra)
{
    const unsigned char *ptr = dra->avctx->extradata;

    return 0;
}




static int dra_decode_frame(AVCodecContext *avctx,
                            void *outbuffer, int *outputsize,
                            AVPacket *avpkt)
{
    const uint8_t *inbuffer = avpkt->data;
    int input_buffer_size = avpkt->size;
    DRAContext *dra = avctx->priv_data;

    return input_buffer_size;
}

static av_cold int dra_decode_init(AVCodecContext * avctx)
{
    DRAContext *dra = avctx->priv_data;
    dra->avctx = avctx;
    dra->numchannels = dra->avctx->channels;
    return 0;
}

static av_cold int dra_decode_close(AVCodecContext *avctx)
{
    DRAContext *dra = avctx->priv_data;
    return 0;
}

AVCodec ff_dra_decoder = {
    "dra",
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_DRA,
    sizeof(DRAContext),
    dra_decode_init,
    NULL,
    dra_decode_close,
    dra_decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("DRA (Dynamic Resolution Adaptation)"),
};
