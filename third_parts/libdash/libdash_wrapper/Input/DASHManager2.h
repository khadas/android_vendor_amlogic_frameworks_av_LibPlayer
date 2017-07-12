/*
 * DASHManager2.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_INPUT_DASHMANAGER2_H_
#define LIBDASH_FRAMEWORK_INPUT_DASHMANAGER2_H_

#include "../Buffer/MediaObjectBuffer.h"
#include "DASHReceiver.h"
#include "libdash.h"
#include "IMPD.h"
#include "../Buffer/IMediaObjectBufferObserver.h"
#include "IDASHReceiverObserver.h"
#include "../Portable/MultiThreading.h"
#include "MediaObjectDecoder.h"
#include "dash_common.h"

namespace libdash
{
    namespace framework
    {
        namespace input
        {
            class DASHManager2 : public IDASHReceiverObserver, public buffer::IMediaObjectBufferObserver
            {
                public:
                    DASHManager2             (uint32_t maxCapacity, dash::mpd::IMPD *mpd, int32_t type, bool dynamic, URL_IRQ_CB cb);
                    virtual ~DASHManager2    ();

                    bool        Start                   (bool abDump);
                    void        Stop                    ();
                    uint32_t    GetPosition             ();
                    void        SetPosition             (uint32_t segmentNumber); // to implement
                    void        SetPositionInMsec       (uint32_t millisec);
                    int 	      GetSegmentDuration();
                    int 	      GetSegmentTimescale();
                    int 	      GetSize();
                    int32_t    GetSessionType();
                    void        Clear                   ();
                    void        ClearTail               ();
                    void        SetRepresentation       (dash::mpd::IMPD *mpd, dash::mpd::IPeriod *period, dash::mpd::IAdaptationSet *adaptationSet, dash::mpd::IRepresentation *representation);
                    void        ConstructRepresentationMap();

                    void        OnSegmentDownloaded     ();
                    void        OnEstimatedBandwidthBps(int64_t bandwidth);
                    void        OnBufferStateChanged    (uint32_t fillstateInPercent);
                    
		      int 		Read (uint8_t *data, size_t len, int *flags);

		      void SetDumpPath(const char * dumpPath){ this->dumpPath = dumpPath;};

                    static void*    DoDumping             (void *dashmanager);
             	  private:

                    std::map<int64_t, dash::mpd::IRepresentation *>    mRep;
                    	
                    buffer::MediaObjectBuffer   *buffer;
                    DASHReceiver                *receiver;
                    uint32_t                    readSegmentCount;
                    uint32_t                    outputSegmentCount;
                    int32_t                     session_type;
                    int64_t                     mBandwidth;
		      bool                        isRunning;
                    bool                        isDynamic;
		      MediaObjectDecoder*		currentMediaDecoder;
		      URL_IRQ_CB			url_irq_cb;
		      dash::mpd::IAdaptationSet    *mAdaptation;  //maybe need to change

		      const char *dumpPath;
        	      THREAD_HANDLE   dumpingThread;
                    bool            isDumping;

                    CRITICAL_SECTION    lockMutex;
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_INPUT_DASHMANAGER_H_ */
