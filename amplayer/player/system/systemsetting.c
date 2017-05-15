

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log_print.h>
#include <codec_type.h>
#include <libavcodec/avcodec.h>
#include "systemsetting.h"
#include <unistd.h>
#include "player_priv.h"

int PlayerSettingIsEnable(const char* path)
{
    char value[1024];
    if (GetSystemSettingString(path, value, NULL) > 0) {
        if ((!strcmp(value, "1") || !strcmp(value, "true") || !strcmp(value, "enable"))) {
            log_print("%s is enabled\n", path);
            return 1;
        }
    }
    log_print("%s is disabled\n", path);
    return 0;
}


float PlayerGetSettingfloat(const char* path)
{
    char value[1024];
    float ret = 0.0;
    if (GetSystemSettingString(path, value, NULL) > 0) {
        if ((sscanf(value, "%f", &ret)) > 0) {
            log_print("%s is set to %f\n", path, ret);
            return ret;
        }
    }
    log_print("%s is not set\n", path);
    return ret;
}

#define FILTER_VFMT_MPEG12  (1 << VFORMAT_MPEG12)
#define FILTER_VFMT_MPEG4   (1 << VFORMAT_MPEG4)
#define FILTER_VFMT_H264    (1 << VFORMAT_H264)
#define FILTER_VFMT_MJPEG   (1 << VFORMAT_MJPEG)
#define FILTER_VFMT_REAL    (1 << VFORMAT_REAL)
#define FILTER_VFMT_JPEG    (1 << VFORMAT_JPEG)
#define FILTER_VFMT_VC1     (1 << VFORMAT_VC1)
#define FILTER_VFMT_AVS     (1 << VFORMAT_AVS)
#define FILTER_VFMT_SW      (1 << VFORMAT_SW)
#define FILTER_VFMT_HMVC    (1 << VFORMAT_H264MVC)
#define FILTER_VFMT_HEVC    (1 << VFORMAT_HEVC)
#define FILTER_VFMT_VP9     (1 << VFORMAT_VP9)


int PlayerGetVFilterFormat(play_para_t*am_p)
{
    signed short video_index = am_p->vstream_info.video_index;
    char value[1024];
    int filter_fmt = 0;
    unsigned int codec_id;

    if (video_index != -1) {
        AVStream *pStream;
        AVCodecContext  *pCodecCtx;
        pStream = am_p->pFormatCtx->streams[video_index];
        pCodecCtx = pStream->codec;
        if (am_p->stream_type == STREAM_ES && pCodecCtx->codec_tag != 0) {
            codec_id = pCodecCtx->codec_tag;
        } else {
            codec_id = pCodecCtx->codec_id;
        }

        if ((pCodecCtx->codec_id == CODEC_ID_H264MVC) && (!am_p->vdec_profile.hmvc_para.exist)) {
            filter_fmt |= FILTER_VFMT_HMVC;
        }
        if ((pCodecCtx->codec_id == CODEC_ID_H264) && (!am_p->vdec_profile.h264_para.exist)) {
            filter_fmt |= FILTER_VFMT_H264;
        }
        if ((pCodecCtx->codec_id == CODEC_ID_HEVC) && (!am_p->vdec_profile.hevc_para.exist)) {
            filter_fmt |= FILTER_VFMT_HEVC;
        }
    }

    if (GetSystemSettingString("media.amplayer.disable-vcodecs", value, NULL) > 0) {
        log_print("[%s:%d]disable_vdec=%s\n", __FUNCTION__, __LINE__, value);
        if (match_types(value, "MPEG12") || match_types(value, "mpeg12")) {
            filter_fmt |= FILTER_VFMT_MPEG12;
        }
        if (match_types(value, "MPEG4") || match_types(value, "mpeg4")) {
            filter_fmt |= FILTER_VFMT_MPEG4;
        }
        if (match_types(value, "H264") || match_types(value, "h264")) {
            filter_fmt |= FILTER_VFMT_H264;
        }
        if (match_types(value, "HEVC") || match_types(value, "hevc")) {
            filter_fmt |= FILTER_VFMT_HEVC;
        }
        if (match_types(value, "MJPEG") || match_types(value, "mjpeg")) {
            filter_fmt |= FILTER_VFMT_MJPEG;
        }
        if (match_types(value, "REAL") || match_types(value, "real")) {
            filter_fmt |= FILTER_VFMT_REAL;
        }
        if (match_types(value, "JPEG") || match_types(value, "jpeg")) {
            filter_fmt |= FILTER_VFMT_JPEG;
        }
        if (match_types(value, "VC1") || match_types(value, "vc1")) {
            filter_fmt |= FILTER_VFMT_VC1;
        }
        if (match_types(value, "AVS") || match_types(value, "avs")) {
            filter_fmt |= FILTER_VFMT_AVS;
        }
        if (match_types(value, "SW") || match_types(value, "sw")) {
            filter_fmt |= FILTER_VFMT_SW;
        }
        if (match_types(value, "HMVC") || match_types(value, "hmvc")) {
            filter_fmt |= FILTER_VFMT_HMVC;
        }
        /*filter by codec id*/
        if (match_types(value, "DIVX3") || match_types(value, "divx3")) {
            if (codec_id == CODEC_TAG_DIV3) {
                filter_fmt |= FILTER_VFMT_MPEG4;
            }
        }
        if (match_types(value, "DIVX4") || match_types(value, "divx4")) {
            if (codec_id == CODEC_TAG_DIV4) {
                filter_fmt |= FILTER_VFMT_MPEG4;
            }
        }
        if (match_types(value, "DIVX5") || match_types(value, "divx5")) {
            if (codec_id == CODEC_TAG_DIV5) {
                filter_fmt |= FILTER_VFMT_MPEG4;
            }
        }
    }
    log_print("[%s:%d]filter_vfmt=%x\n", __FUNCTION__, __LINE__, filter_fmt);
    return filter_fmt;
}

#define FILTER_AFMT_MPEG        (1 << 0)
#define FILTER_AFMT_PCMS16L     (1 << 1)
#define FILTER_AFMT_AAC         (1 << 2)
#define FILTER_AFMT_AC3         (1 << 3)
#define FILTER_AFMT_ALAW        (1 << 4)
#define FILTER_AFMT_MULAW       (1 << 5)
#define FILTER_AFMT_DTS         (1 << 6)
#define FILTER_AFMT_PCMS16B     (1 << 7)
#define FILTER_AFMT_FLAC        (1 << 8)
#define FILTER_AFMT_COOK        (1 << 9)
#define FILTER_AFMT_PCMU8       (1 << 10)
#define FILTER_AFMT_ADPCM       (1 << 11)
#define FILTER_AFMT_AMR         (1 << 12)
#define FILTER_AFMT_RAAC        (1 << 13)
#define FILTER_AFMT_WMA         (1 << 14)
#define FILTER_AFMT_WMAPRO      (1 << 15)
#define FILTER_AFMT_PCMBLU      (1 << 16)
#define FILTER_AFMT_ALAC        (1 << 17)
#define FILTER_AFMT_VORBIS      (1 << 18)
#define FILTER_AFMT_AAC_LATM    (1 << 19)
#define FILTER_AFMT_APE         (1 << 20)
#define FILTER_AFMT_EAC3        (1 << 21)
#define FILTER_AFMT_DRA         (1 << 23)
#define FILTER_AFMT_TRUEHD      (1 << 25)

int PlayerGetAFilterFormat(const char *prop)
{
    char value[1024];
    int filter_fmt = 0;
#ifndef     USE_ARM_AUDIO_DEC
    /* check the dts/ac3 firmware status */
    if (access("/system/etc/firmware/audiodsp_codec_ddp_dcv.bin", F_OK)) {
        //#ifndef DOLBY_DAP_EN
        if (access("/system/lib/libstagefright_soft_ddpdec.so", F_OK)) {
            filter_fmt |= (FILTER_AFMT_AC3 | FILTER_AFMT_EAC3);
        }
        //#endif
    }
    if (access("/system/etc/firmware/audiodsp_codec_dtshd.bin", F_OK)) {
        if (access("/system/lib/libstagefright_soft_dtshd.so", F_OK)) {
            filter_fmt  |= FILTER_AFMT_DTS;
        }
    }
#else
    if (access("/system/lib/libstagefright_soft_dcvdec.so", F_OK)) {
#ifndef DOLBY_DAP_EN
        filter_fmt |= (FILTER_AFMT_AC3 | FILTER_AFMT_EAC3);
#endif
    }
    if (access("/system/lib/libstagefright_soft_dtshd.so", F_OK)) {
        filter_fmt  |= FILTER_AFMT_DTS;
    }
    if (access("/system/lib/libdra.so", F_OK)) {
        filter_fmt |= FILTER_AFMT_DRA;
    }
#endif
    if (GetSystemSettingString(prop, value, NULL) > 0) {
        log_print("[%s:%d]disable_adec=%s\n", __FUNCTION__, __LINE__, value);
        if (match_types(value, "mpeg") || match_types(value, "MPEG")) {
            filter_fmt |= FILTER_AFMT_MPEG;
        }
        if (match_types(value, "pcms16l") || match_types(value, "PCMS16L")) {
            filter_fmt |= FILTER_AFMT_PCMS16L;
        }
        if (match_types(value, "aac") || match_types(value, "AAC")) {
            filter_fmt |= FILTER_AFMT_AAC;
        }
        if (match_types(value, "ac3") || match_types(value, "AC3")) {
            filter_fmt |= FILTER_AFMT_AC3;
        }
        if (match_types(value, "alaw") || match_types(value, "ALAW")) {
            filter_fmt |= FILTER_AFMT_ALAW;
        }
        if (match_types(value, "mulaw") || match_types(value, "MULAW")) {
            filter_fmt |= FILTER_AFMT_MULAW;
        }
        if (match_types(value, "dts") || match_types(value, "DTS")) {
            filter_fmt |= FILTER_AFMT_DTS;
        }
        if (match_types(value, "pcms16b") || match_types(value, "PCMS16B")) {
            filter_fmt |= FILTER_AFMT_PCMS16B;
        }
        if (match_types(value, "flac") || match_types(value, "FLAC")) {
            filter_fmt |= FILTER_AFMT_FLAC;
        }
        if (match_types(value, "cook") || match_types(value, "COOK")) {
            filter_fmt |= FILTER_AFMT_COOK;
        }
        if (match_types(value, "pcmu8") || match_types(value, "PCMU8")) {
            filter_fmt |= FILTER_AFMT_PCMU8;
        }
        if (match_types(value, "adpcm") || match_types(value, "ADPCM")) {
            filter_fmt |= FILTER_AFMT_ADPCM;
        }
        if (match_types(value, "amr") || match_types(value, "AMR")) {
            filter_fmt |= FILTER_AFMT_AMR;
        }
        if (match_types(value, "raac") || match_types(value, "RAAC")) {
            filter_fmt |= FILTER_AFMT_RAAC;
        }
        if (match_types(value, "wma") || match_types(value, "WMA")) {
            filter_fmt |= FILTER_AFMT_WMA;
        }
        if (match_types(value, "wmapro") || match_types(value, "WMAPRO")) {
            filter_fmt |= FILTER_AFMT_WMAPRO;
        }
        if (match_types(value, "pcmblueray") || match_types(value, "PCMBLUERAY")) {
            filter_fmt |= FILTER_AFMT_PCMBLU;
        }
        if (match_types(value, "alac") || match_types(value, "ALAC")) {
            filter_fmt |= FILTER_AFMT_ALAC;
        }
        if (match_types(value, "vorbis") || match_types(value, "VORBIS")) {
            filter_fmt |= FILTER_AFMT_VORBIS;
        }
        if (match_types(value, "aac_latm") || match_types(value, "AAC_LATM")) {
            filter_fmt |= FILTER_AFMT_AAC_LATM;
        }
        if (match_types(value, "ape") || match_types(value, "APE")) {
            filter_fmt |= FILTER_AFMT_APE;
        }
        if (match_types(value, "eac3") || match_types(value, "EAC3")) {
            filter_fmt |= FILTER_AFMT_EAC3;
        }
        if (match_types(value, "TrueHD") || match_types(value, "TRUEHD")) {
            filter_fmt |= FILTER_AFMT_TRUEHD;
        }
        if (match_types(value, "dra") != NULL || match_types(value, "DRA") != NULL) {
            filter_fmt |= FILTER_AFMT_DRA;
        }
    }
    log_print("[%s:%d]filter_afmt=%x\n", __FUNCTION__, __LINE__, filter_fmt);
    return filter_fmt;
}


