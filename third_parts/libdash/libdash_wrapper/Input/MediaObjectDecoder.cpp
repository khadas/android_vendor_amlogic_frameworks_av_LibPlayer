/*
 * MediaObjectDecoder.cpp
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "MediaObjectDecoder.h"

using namespace libdash::framework::input;
using namespace dash::mpd;

MediaObjectDecoder::MediaObjectDecoder  (MediaObject* initSegment, MediaObject* mediaSegment) :
                    initSegment         (initSegment),
                    mediaSegment        (mediaSegment),
                    decoderInitialized  (false),
                    initSegmentOffset   (0)
{
}
MediaObjectDecoder::~MediaObjectDecoder()
{
    delete this->mediaSegment;
}

bool MediaObjectDecoder::isFAIL()
{
    return this->mediaSegment->isFAIL();
}

int     MediaObjectDecoder::Read                    (uint8_t *buf, int buf_size)
{
    int ret = 0;
    if (!this->decoderInitialized && this->initSegment)
    {
        ret = this->initSegment->Peek(buf, buf_size, this->initSegmentOffset);
        this->initSegmentOffset += (size_t) ret;
        
        if(ret == 0)
        	this->decoderInitialized = true;
    }

    if (ret == 0)
        ret = this->mediaSegment->Read(buf, buf_size);

    return ret;
}
