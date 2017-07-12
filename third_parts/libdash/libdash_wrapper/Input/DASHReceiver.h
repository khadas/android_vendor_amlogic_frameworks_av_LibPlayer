/*
 * DASHReceiver.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_
#define LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_

#include "libdash.h"
#include "IMPD.h"

#include "IDASHReceiverObserver.h"
#include "../Buffer/MediaObjectBuffer.h"
#include "../MPD/AdaptationSetStream.h"
#include "../MPD/IRepresentationStream.h"
#include "../Portable/MultiThreading.h"

namespace libdash
{
    namespace framework
    {
        namespace input
        {
            class DASHReceiver
            {
                public:
                    DASHReceiver            (dash::mpd::IMPD *mpd, IDASHReceiverObserver *obs, buffer::MediaObjectBuffer *buffer, uint32_t bufferSize);
                    virtual ~DASHReceiver   ();

                    bool                        Start                   ();
                    void                        Stop                    ();
                    input::MediaObject*         GetNextSegment          ();
                    input::MediaObject*         GetSegment              (uint32_t segmentNumber);
                    input::MediaObject*         GetInitSegment          ();
                    input::MediaObject*         FindInitSegment         (dash::mpd::IRepresentation *representation);
                    uint32_t                    GetPosition             ();
                    void                        SetPosition             (uint32_t segmentNumber);
                    void                        SetPositionInMsecs      (uint32_t milliSecs);
                    dash::mpd::IRepresentation* GetRepresentation       ();
                    void                        SetRepresentation       (dash::mpd::IMPD *mpd,
                                                                         dash::mpd::IPeriod *period,
                                                                         dash::mpd::IAdaptationSet *adaptationSet,
                                                                         dash::mpd::IRepresentation *representation);

                    int	GetSegmentDuration();
                    int 	GetSegmentTimescale();
                    int 	GetSize();			// the sum of segment

                private:
                    int64_t         GetBufferDuration();
                    uint32_t        CalculateSegmentOffset  ();
                    void            NotifySegmentDownloaded ();
                    void            NotifyEstimatedBandwidthBps(int64_t bandwidth);
                    void            DownloadInitSegment     (dash::mpd::IRepresentation* rep);
                    bool            InitSegmentExists       (dash::mpd::IRepresentation* rep);

                    static void*    DoBuffering             (void *receiver);

                    int64_t anchorTimeS;
                    int64_t retryDurationS;

                    std::map<dash::mpd::IRepresentation*, MediaObject*> initSegments;
                    buffer::MediaObjectBuffer                           *buffer;
                    IDASHReceiverObserver                               *observer;
                    dash::mpd::IMPD                                     *mpd;
                    dash::mpd::IPeriod                                  *period;
                    dash::mpd::IAdaptationSet                           *adaptationSet;
                    dash::mpd::IRepresentation                          *representation;
                    mpd::AdaptationSetStream                            *adaptationSetStream;
                    mpd::IRepresentationStream                          *representationStream;

                    std::vector<dash::mpd::IMPD *>    v_mpd;
                    std::vector<dash::mpd::IPeriod *>    v_period;
                    std::vector<dash::mpd::IAdaptationSet *>    v_adaptationSet;
                    std::vector<dash::mpd::IRepresentation *>    v_representation;
                    std::vector<mpd::AdaptationSetStream *>    v_adaptationSetStream;
                    std::vector<mpd::IRepresentationStream *>    v_representationStream;

                    uint32_t                                            numberCounter;
                    uint32_t                                            segmentNumber;
                    uint32_t                                            m_segmentNumber;
                    uint32_t                                            positionInMsecs;
                    uint32_t                                            segmentOffset;
                    uint32_t                                            bufferSize;
                    CRITICAL_SECTION                                    monitorMutex;

                    THREAD_HANDLE   bufferingThread;
                    bool            isBuffering;

		      CRITICAL_SECTION                                    currentmediaMutex;
                    MediaObject * currentmedia; 
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_INPUT_DASHRECEIVER_H_ */
