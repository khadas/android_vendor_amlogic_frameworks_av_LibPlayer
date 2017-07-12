/*
 * MediaObjectDecoder.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECTDECODER_H_
#define LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECTDECODER_H_

#include "MediaObject.h"
#include "IDataReceiver.h"

namespace libdash
{
    namespace framework
    {
        namespace input
        {
            class MediaObjectDecoder : public IDataReceiver
            	{
                public:
                    MediaObjectDecoder          (MediaObject *initSeg, MediaObject *mediaSeg); 
                    virtual ~MediaObjectDecoder ();

                    virtual int     Read                    (uint8_t *buf, int buf_size);
                    virtual bool    isFAIL ();


                private:
                    MediaObject                         *initSegment;
                    MediaObject                         *mediaSegment;
                    bool                                decoderInitialized;
                    size_t                              initSegmentOffset;
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECTDECODER_H_ */
