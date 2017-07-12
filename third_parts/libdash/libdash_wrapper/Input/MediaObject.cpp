/*
 * MediaObject.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "MediaObject.h"

using namespace libdash::framework::input;

using namespace dash::mpd;
using namespace dash::network;
using namespace dash::metrics;

MediaObject::MediaObject    (ISegment *segment, IRepresentation *rep) :
             segment        (segment),
             rep            (rep),
             state(NOT_STARTED),
             mBandwidth(0)
{
    InitializeConditionVariable (&this->stateChanged);
    InitializeCriticalSection   (&this->stateLock);
    InitializeCriticalSection   (&this->finishLock);
}
MediaObject::~MediaObject   ()
{
    if(this->state == IN_PROGRESS)
    {
        this->segment->AbortDownload();
        this->OnDownloadStateChanged(ABORTED);
    }

    // make sure that WaitFinished() returned correctly.
    // prevent blocking in WaitFinished().
    EnterCriticalSection(&this->finishLock);
    LeaveCriticalSection(&this->finishLock);

    this->segment->DetachDownloadObserver(this);
    int64_t dummy = 0;
    this->WaitFinished(&dummy);

    delete segment;

    DeleteConditionVariable (&this->stateChanged);
    DeleteCriticalSection   (&this->stateLock);
    DeleteCriticalSection   (&this->finishLock);
}

bool                MediaObject::isFAIL()
{
    return (this->state == FAIL);
}

bool                MediaObject::StartDownload          ()
{
    this->state=STARTED;
    this->segment->AttachDownloadObserver(this);
    return this->segment->StartDownload();
}
bool			MediaObject::IsStarted()
{
	return (this->state >= STARTED && this->state < FAIL);
}
void                MediaObject::AbortDownload          ()
{
    this->segment->AbortDownload();
    this->OnDownloadStateChanged(ABORTED);
}
void                MediaObject::WaitFinished           (int64_t * bandwidth)
{
    EnterCriticalSection(&this->finishLock);
    EnterCriticalSection(&this->stateLock);

    while(this->state != NOT_STARTED && this->state != COMPLETED && this->state != ABORTED && this->state != FAIL)
        SleepConditionVariableCS(&this->stateChanged, &this->stateLock, INFINITE);

    *bandwidth = this->mBandwidth;
    LeaveCriticalSection(&this->stateLock);
    LeaveCriticalSection(&this->finishLock);
}
int 			MediaObject::GetSegmentNum()
{
	return this->segment->GetSegmentNum();
}
int                 MediaObject::Read                   (uint8_t *data, size_t len)
{
    return this->segment->Read(data, len);
}
int                 MediaObject::Peek                   (uint8_t *data, size_t len)
{
    return this->segment->Peek(data, len);
}
int                 MediaObject::Peek                   (uint8_t *data, size_t len, size_t offset)
{
    return this->segment->Peek(data, len, offset);
}
int64_t    MediaObject::getBandwidthBps()
{
    return this->mBandwidth;
}
IRepresentation*    MediaObject::GetRepresentation      ()
{
    return this->rep;
}
void                MediaObject::OnDownloadStateChanged (DownloadState state)
{
    EnterCriticalSection(&this->stateLock);

    this->state = state;
    
    WakeAllConditionVariable(&this->stateChanged);
    LeaveCriticalSection(&this->stateLock);
}
void                MediaObject::OnDownloadRateChanged  (uint64_t bytesDownloaded)
{
}

void    MediaObject::OnDownloadBandwidthBps(int64_t bandwidth)
{
    this->mBandwidth = bandwidth;
    LOGI("[%s:%d] download bandwidth : %lld bps", __FUNCTION__, __LINE__, bandwidth);
}

const std::vector<ITCPConnection *>&    MediaObject::GetTCPConnectionList   () const
{
    return this->segment->GetTCPConnectionList();
}
const std::vector<IHTTPTransaction *>&  MediaObject::GetHTTPTransactionList () const
{
    return this->segment->GetHTTPTransactionList();
}
