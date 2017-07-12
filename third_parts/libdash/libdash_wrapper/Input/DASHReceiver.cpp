/*
 * DASHReceiver.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHReceiver.h"
#include "libdash_extern_api.h"

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace libdash::framework::mpd;
using namespace dash::mpd;
using namespace std;
#include<sys/time.h>
#include <time.h>
int64_t indash_gettimeUs(void){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

DASHReceiver::DASHReceiver          (IMPD *mpd, IDASHReceiverObserver *obs, MediaObjectBuffer *buffer, uint32_t bufferSize) :
              mpd                   (mpd),
              period                (NULL),
              adaptationSet         (NULL),
              representation        (NULL),
              adaptationSetStream   (NULL),
              representationStream  (NULL),
              anchorTimeS           (0),
              retryDurationS        (0),
              numberCounter         (0),
              segmentNumber         (0),
              segmentOffset         (0),
              m_segmentNumber     (0),
              observer              (obs),
              buffer                (buffer),
              bufferSize            (bufferSize),
              isBuffering           (false),
              currentmedia		(NULL)
{
    this->period                = this->mpd->GetPeriods().at(0);
    this->adaptationSet         = this->period->GetAdaptationSets().at(0);
    this->representation        = this->adaptationSet->GetRepresentation().at(0);

    this->adaptationSetStream   = new AdaptationSetStream(mpd, period, adaptationSet);
    this->representationStream  = adaptationSetStream->GetRepresentationStream(this->representation);

    if (this->mpd->GetType() == "dynamic" && (!(this->mpd->GetAvailabilityStarttime()).empty()) && this->GetSegmentDuration() > 0) {
        this->numberCounter = this->representationStream->GetFirstSegmentNumber();
        this->segmentOffset = this->numberCounter;
    }

    InitializeCriticalSection(&this->monitorMutex);
    InitializeCriticalSection(&this->currentmediaMutex);
    LOGI("[%s:%d]segmentOffset=%d \n",__FUNCTION__,__LINE__,this->segmentOffset);
}
DASHReceiver::~DASHReceiver     ()
{

    if (this->mpd) {
        this->mpd->Delete();
    }
    std::map<IRepresentation*, MediaObject*>::iterator iter;
    for (iter = initSegments.begin(); iter != initSegments.end(); ++iter)
    {
        delete(iter->second);
    }

    delete this->adaptationSetStream;

    int32_t i = 0;
    if (!v_mpd.empty()) {
        for (i = 0; i < v_mpd.size(); i++) {
            v_mpd.at(i)->Delete();
        }
    }
    if (!v_adaptationSetStream.empty()) {
        for (i = 0; i < v_adaptationSetStream.size(); i++) {
            delete v_adaptationSetStream.at(i);
        }
    }

    DeleteCriticalSection(&this->currentmediaMutex);
    DeleteCriticalSection(&this->monitorMutex);
    TRACE();
}

bool                        DASHReceiver::Start                     ()
{
    if(this->isBuffering)
        return false;

    LOGI("[%s:%d]size=%d,  segmentNumber=%d, segmentOffset=%d\n",__FUNCTION__,__LINE__, this->representationStream->GetSize(), this->segmentNumber, this->segmentOffset);
    this->buffer->SetEOS(false);
    this->isBuffering       = true;
    this->bufferingThread   = CreateThreadPortable (DoBuffering, this);

    if(this->bufferingThread == NULL)
    {
        this->isBuffering = false;
        return false;
    }

    return true;
}
void                        DASHReceiver::Stop                      ()
{
    if(!this->isBuffering)
        return;

    this->isBuffering = false;
    this->buffer->SetEOS(true);

    // abort the current download. 
    EnterCriticalSection(&this->currentmediaMutex);
    if(this->currentmedia != NULL)
    	this->currentmedia->AbortDownload();
    LeaveCriticalSection(&this->currentmediaMutex);
    
    if(this->bufferingThread != NULL)
    {
        JoinThread(this->bufferingThread);
        DestroyThreadPortable(this->bufferingThread);
    }
    LOGI("[%s:%d]doing successful! \n",__FUNCTION__,__LINE__);
}
MediaObject*                DASHReceiver::GetNextSegment            ()
{
    ISegment *seg = NULL;

    EnterCriticalSection(&this->monitorMutex);

    IMPD * t_mpd = NULL;
    IPeriod * t_period = NULL;
    IAdaptationSet * t_adaptationSet = NULL;
    IRepresentation * t_representation = NULL;
    AdaptationSetStream * t_adaptationSetStream = NULL;
    IRepresentationStream * t_representationStream = NULL;

    bool refresh_flag = false;
    if (v_mpd.size() > 0) {
        refresh_flag = true;
    }

    if (refresh_flag) { // got refreshed mpd.
        t_mpd = v_mpd.at(0);
        t_period = v_period.at(0);
        t_adaptationSet = v_adaptationSet.at(0);
        t_representation = v_representation.at(0);
        t_adaptationSetStream = v_adaptationSetStream.at(0);
        t_representationStream = v_representationStream.at(0);

        // pop front
        v_mpd.erase(v_mpd.begin());
        v_period.erase(v_period.begin());
        v_adaptationSet.erase(v_adaptationSet.begin());
        v_representation.erase(v_representation.begin());
        v_adaptationSetStream.erase(v_adaptationSetStream.begin());
        v_representationStream.erase(v_representationStream.begin());
    }

    if (this->mpd->GetType() == "dynamic") {
        string mini_up = this->mpd->GetMinimumUpdatePeriod();
        if (!t_representationStream) {
            t_representationStream = this->representationStream;
        }
        if (this->representationStream->HasTimeLineOrNot()) {
            bool trickflag = false;
            if (this->segmentNumber >= this->representationStream->GetSize()) {
                this->segmentNumber = this->representationStream->GetSize() -1;
                trickflag = true;
            }
            uint64_t t_startTime = this->representationStream->GetSegmentStartTimeByOffset(this->segmentNumber);
            if (!t_startTime) {
                this->segmentNumber = 0;
            } else {
                uint32_t t_offSet = t_representationStream->GetSegmentOffsetByTime(t_startTime);
                if (t_offSet < 0) {
                    this->segmentNumber = 0;
                } else {
                    if (trickflag) {
                        this->segmentNumber = ++t_offSet;
                    } else {
                        this->segmentNumber = t_offSet;
                    }
                }
            }
        } else if (numberCounter > 0) {
            // nothing to do yet.
        } else {
            // just temporary.
            uint32_t new_nb = t_representationStream->GetStartNumber();
            if (new_nb > this->m_segmentNumber) { // reset segmentNumber
                this->segmentNumber = 0;
                this->segmentOffset = 0;
                this->m_segmentNumber = new_nb;
            } else {
                this->segmentNumber = 0;
                this->segmentOffset++;
                this->m_segmentNumber++;
            }
        }
    }
    LOGI("[%s:%d] find next segment offset in refreshed mpd : %d\n", __FUNCTION__, __LINE__, this->segmentNumber);

    if (refresh_flag) {
        // release old member
        if (this->mpd) {
            this->mpd->Delete();
        }
        if (this->adaptationSetStream) {
            delete this->adaptationSetStream;
        }

        this->mpd = t_mpd;
        this->period = t_period;
        this->adaptationSet = t_adaptationSet;
        this->representation = t_representation;
        this->adaptationSetStream = t_adaptationSetStream;
        this->representationStream = t_representationStream;

        DownloadInitSegment(this->representation);
    }

    LeaveCriticalSection(&this->monitorMutex);

    LOGI("[%s:%d] segmentNumber=%d, segmentOffset=%d, representationStream size=%d\n", __FUNCTION__, __LINE__, this->segmentNumber, this->segmentOffset, this->representationStream->GetSize());

    if((this->segmentNumber >= this->representationStream->GetSize() || this->segmentNumber < 0) && numberCounter <= 0)
        return NULL;
    
    seg = this->representationStream->GetMediaSegment(this->segmentNumber + this->segmentOffset);

    if (seg != NULL)
    {
        MediaObject *media = new MediaObject(seg, this->representation);
        this->segmentNumber++;
        return media;
    }

    return NULL;
}
MediaObject*                DASHReceiver::GetSegment                (uint32_t segNum)
{
    ISegment *seg = NULL;

    if(segNum >= this->representationStream->GetSize())
        return NULL;

    LOGI("[%s:%d]segNum=%d,segmentOffset=%d\n",__FUNCTION__,__LINE__,segNum,segmentOffset);
    seg = this->representationStream->GetMediaSegment(segNum + segmentOffset);

    if (seg != NULL)
    {
        MediaObject *media = new MediaObject(seg, this->representation);
        return media;
    }

    return NULL;
}
MediaObject*                DASHReceiver::GetInitSegment            ()
{
    ISegment *seg = NULL;

    seg = this->representationStream->GetInitializationSegment();
    
    if(NULL == seg)
    	seg = this->adaptationSetStream->GetInitializationSegment();

    if (seg != NULL)
    {
        MediaObject *media = new MediaObject(seg, this->representation);
        return media;
    }

    return NULL;
}
MediaObject*                DASHReceiver::FindInitSegment           (dash::mpd::IRepresentation *representation)
{
    if (!this->InitSegmentExists(representation))
        return NULL;

    return this->initSegments[representation];
}
uint32_t                    DASHReceiver::GetPosition               ()
{
    return this->segmentNumber;
}
void                        DASHReceiver::SetPosition               (uint32_t segmentNumber)
{
    // some logic here

    this->segmentNumber = segmentNumber;
}
void                        DASHReceiver::SetPositionInMsecs        (uint32_t milliSecs)
{
    // some logic here

    this->positionInMsecs = milliSecs;
}
void                        DASHReceiver::SetRepresentation         (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
    EnterCriticalSection(&this->monitorMutex);

    bool periodChanged = false;

    // mpd changed in "dynamic" only, cache those elements.
    // noblock
    if (mpd) {

        AdaptationSetStream * t_adaptationSetStream = new AdaptationSetStream(mpd, period, adaptationSet);
        IRepresentationStream * t_representationStream = t_adaptationSetStream->GetRepresentationStream(representation);
        v_mpd.push_back(mpd);
        v_period.push_back(period);
        v_adaptationSet.push_back(adaptationSet);
        v_representation.push_back(representation);
        v_adaptationSetStream.push_back(t_adaptationSetStream);
        v_representationStream.push_back(t_representationStream);

        //DownloadInitSegment(representation);

    } else {

        if (!representation || this->representation == representation)
        {
            LeaveCriticalSection(&this->monitorMutex);
            return;
        }

        this->representation = representation;

        if (adaptationSet && this->adaptationSet != adaptationSet)
        {
            this->adaptationSet = adaptationSet;

            if (period && this->period != period)
            {
                this->period = period;
                periodChanged = true;
            }

            delete this->adaptationSetStream;
            this->adaptationSetStream = NULL;

            this->adaptationSetStream = new AdaptationSetStream(this->mpd, this->period, this->adaptationSet);
        }

        this->representationStream  = this->adaptationSetStream->GetRepresentationStream(this->representation);
        this->DownloadInitSegment(this->representation);

        if (periodChanged)
        {
            this->segmentNumber = 0;
            this->segmentOffset = this->CalculateSegmentOffset();
            if (this->numberCounter > 0) {
                this->numberCounter = this->representationStream->GetFirstSegmentNumber();
                this->segmentOffset = this->numberCounter;
            }
        }
    }

    LeaveCriticalSection(&this->monitorMutex);
}
int DASHReceiver::GetSegmentDuration(){
	return this->representationStream->GetAverageSegmentDuration();
}
int DASHReceiver::GetSegmentTimescale(){
	return this->representationStream->GetSegmentTimescale();
}
int DASHReceiver::GetSize(){
	return this->representationStream->GetSize();
}
dash::mpd::IRepresentation* DASHReceiver::GetRepresentation         ()
{
    return this->representation;
}

int64_t DASHReceiver::GetBufferDuration()
{
    int64_t mbt = (int64_t)(TimeResolver::GetDurationInSec(this->mpd->GetMinBufferTime()));
    if (mbt <= 0) { // maybe invalid, try segment duration.
        mbt = this->GetSegmentDuration() / this->GetSegmentTimescale();
    }
    return mbt;
}

uint32_t                    DASHReceiver::CalculateSegmentOffset    ()
{
    return 0; // anyway
#if 0
    if (mpd->GetType() == "static")
        return 0;

    uint32_t firstSegNum = this->representationStream->GetFirstSegmentNumber();
    uint32_t currSegNum  = this->representationStream->GetCurrentSegmentNumber();
    uint32_t startSegNum;

    if (!currSegNum) {
        startSegNum = 0;
    } else {
        startSegNum = currSegNum - 2*bufferSize;
    }

    return (startSegNum > firstSegNum) ? startSegNum : firstSegNum;
#endif
}
void                        DASHReceiver::NotifySegmentDownloaded   ()
{
    this->observer->OnSegmentDownloaded();
}

void    DASHReceiver::NotifyEstimatedBandwidthBps(int64_t bandwidth)
{
    this->observer->OnEstimatedBandwidthBps(bandwidth);
}

void                        DASHReceiver::DownloadInitSegment    (IRepresentation* rep)
{
    if (this->InitSegmentExists(rep))
        return;

    MediaObject *initSeg = NULL;
    initSeg = this->GetInitSegment();

    if (initSeg)
    {
        initSeg->StartDownload();
        this->initSegments[rep] = initSeg;
    }
}
bool                        DASHReceiver::InitSegmentExists      (IRepresentation* rep)
{
    if (this->initSegments.find(rep) != this->initSegments.end())
        return true;

    return false;
}

/* Thread that does the buffering of segments */
void*                       DASHReceiver::DoBuffering               (void *receiver)
{
    LOGI("[%s:%d]thread start!!! \n",__FUNCTION__,__LINE__);
    DASHReceiver *dashReceiver = (DASHReceiver *) receiver;

    dashReceiver->DownloadInitSegment(dashReceiver->GetRepresentation());
    
    EnterCriticalSection(&dashReceiver->currentmediaMutex);
    dashReceiver->currentmedia = dashReceiver->GetNextSegment();
    LeaveCriticalSection(&dashReceiver->currentmediaMutex);

    bool retry_flag = false;

    while(dashReceiver->isBuffering && !extern_interrupt_cb())
    {
        if (!dashReceiver->currentmedia) {
            if (dashReceiver->mpd->GetType() == "static") {
                break;
            } else {
                PortableSleepMs(100);
                LOGI("[%s:%d] no valid mediaObject!\n", __FUNCTION__, __LINE__);
                EnterCriticalSection(&dashReceiver->currentmediaMutex);
                dashReceiver->currentmedia = dashReceiver->GetNextSegment();
                LeaveCriticalSection(&dashReceiver->currentmediaMutex);
                continue;
            }
        }
        if(!dashReceiver->currentmedia->IsStarted()){
            dashReceiver->currentmedia->StartDownload();
        }

        if (dashReceiver->currentmedia->isFAIL()) {
            if (!retry_flag) {
                dashReceiver->anchorTimeS = indash_gettimeUs() / 1000000;
                dashReceiver->retryDurationS = dashReceiver->GetBufferDuration();
                retry_flag = true;
            }
            if (indash_gettimeUs() / 1000000 - dashReceiver->anchorTimeS < dashReceiver->retryDurationS) {
                LOGI("[%s:%d] mediaobject download failed, maybe need to retry! retry duration : %lld s\n", __FUNCTION__,__LINE__, dashReceiver->retryDurationS);
                PortableSleepMs(100);
                continue;
            } else {
                retry_flag = false;
                EnterCriticalSection(&dashReceiver->currentmediaMutex);
                delete dashReceiver->currentmedia;
                dashReceiver->currentmedia = NULL;
                LeaveCriticalSection(&dashReceiver->currentmediaMutex);
                continue;
            }
        }

        if (!dashReceiver->buffer->PushBack(dashReceiver->currentmedia)){
            // the buffer queue is "maxCapacity"
            PortableSleepMs(100);
            continue;
        }

        LOGI("[%s:%d]PushBack segment=%d \n",__FUNCTION__,__LINE__,dashReceiver->currentmedia->GetSegmentNum());

        int64_t bandwidth = 0;
        dashReceiver->currentmedia->WaitFinished(&bandwidth);
        dashReceiver->NotifySegmentDownloaded();
        dashReceiver->NotifyEstimatedBandwidthBps(bandwidth);
        LOGI("[%s:%d] ******* Download finished ********** \n",__FUNCTION__,__LINE__);
        EnterCriticalSection(&dashReceiver->currentmediaMutex);
        dashReceiver->currentmedia=NULL;

        if(!dashReceiver->isBuffering){
            LeaveCriticalSection(&dashReceiver->currentmediaMutex);
            break;
        }
        dashReceiver->currentmedia = dashReceiver->GetNextSegment();
        LeaveCriticalSection(&dashReceiver->currentmediaMutex);
    }

    EnterCriticalSection(&dashReceiver->currentmediaMutex);
    if(dashReceiver->currentmedia != NULL)
    {
        delete dashReceiver->currentmedia;
        dashReceiver->currentmedia = NULL;
    }
    LeaveCriticalSection(&dashReceiver->currentmediaMutex);
    dashReceiver->buffer->SetEOS(true);

    LOGI("[%s:%d]thread end, and set the cache buffer to EOS!!! \n",__FUNCTION__,__LINE__);
    return NULL;
}
