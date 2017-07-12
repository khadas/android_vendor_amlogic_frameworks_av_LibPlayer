/*
 * MediaObject.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECT_H_
#define LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECT_H_

#include "IMPD.h"
#include "IDownloadObserver.h"
#include "IDASHMetrics.h"
#include "../Portable/MultiThreading.h"

namespace libdash
{
    namespace framework
    {
        namespace input
        {
            class MediaObject : public dash::network::IDownloadObserver, public dash::metrics::IDASHMetrics
            {
                public:
                    MediaObject             (dash::mpd::ISegment *segment, dash::mpd::IRepresentation *rep);
                    virtual ~MediaObject    ();

                    bool                        isFAIL();
                    bool                        StartDownload       ();
                    bool				IsStarted();
                    void                        AbortDownload       ();
                    /***
                    Got bandwidth from this function due to abnormal crashing when mediaobject been released in DASHManager2.
                    ***/
                    void                        WaitFinished        (int64_t * bandwidth);
                    int 				GetSegmentNum();
                    int                         Read                (uint8_t *data, size_t len);
                    int                         Peek                (uint8_t *data, size_t len);
                    int                         Peek                (uint8_t *data, size_t len, size_t offset);
                    int64_t    getBandwidthBps();
                    dash::mpd::IRepresentation* GetRepresentation   ();

                    virtual void    OnDownloadStateChanged  (dash::network::DownloadState state);
                    virtual void    OnDownloadRateChanged   (uint64_t bytesDownloaded);
                    virtual void    OnDownloadBandwidthBps (int64_t bandwidth);
                    /*
                     * IDASHMetrics
                     */
                    const std::vector<dash::metrics::ITCPConnection *>&     GetTCPConnectionList    () const;
                    const std::vector<dash::metrics::IHTTPTransaction *>&   GetHTTPTransactionList  () const;

                private:
                    dash::mpd::ISegment             *segment;
                    dash::mpd::IRepresentation      *rep;
                    dash::network::DownloadState    state;

                    mutable CRITICAL_SECTION    stateLock;
                    mutable CRITICAL_SECTION    finishLock;
                    mutable CONDITION_VARIABLE  stateChanged;
                    int64_t    mBandwidth;
            };
        }
    }
}

#endif /* LIBDASH_FRAMEWORK_INPUT_MEDIAOBJECT_H_ */
