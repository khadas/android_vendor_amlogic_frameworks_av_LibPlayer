/*
 * dra stream defines
 * Copyright (c) 2016 lianlian zhu
 *
 */



#include "avformat.h"

#define NB_PID_MAX 8192
#define MAX_SECTION_SIZE 4096



typedef struct DraContext DraContext;

DraContext *ff_dra_parse_open(AVFormatContext *s);
int ff_dra_parse_packet(DraContext *dra, AVPacket *pkt,
                        const uint8_t *buf, int len);
void ff_dra_parse_close(DraContext *ts);

int ff_parse_dra_descriptor(AVFormatContext *fc, AVStream *st, int stream_type,
                            const uint8_t **pp, const uint8_t *desc_list_end,
                            int mp4_dec_config_descr_len, int mp4_es_id, int pid,
                            uint8_t *mp4_dec_config_descr);

