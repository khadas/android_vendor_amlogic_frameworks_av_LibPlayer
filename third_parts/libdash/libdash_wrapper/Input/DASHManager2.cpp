/*
 * DASHManager2.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHManager2.h"
#include <stdio.h>

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;

using namespace dash;
using namespace dash::network;
using namespace dash::mpd;
using namespace std;

DASHManager2::DASHManager2        (uint32_t maxCapacity, IMPD* mpd, int32_t type, bool dynamic, URL_IRQ_CB cb) :
             readSegmentCount   (0),
             outputSegmentCount (0),
             mBandwidth(0),
             session_type(type),
             receiver           (NULL),
             isRunning          (false),
             isDynamic         (dynamic),
             dumpingThread   (NULL),
             isDumping		(false),
             currentMediaDecoder(NULL),
             dumpPath(NULL), 
             url_irq_cb(cb)
{
    InitializeCriticalSection(&this->lockMutex);
    this->buffer    = new MediaObjectBuffer(maxCapacity);
    this->buffer->AttachObserver(this);

    this->receiver  = new DASHReceiver(mpd, this, this->buffer, maxCapacity);
}
DASHManager2::~DASHManager2       ()
{
    this->Stop();
    delete this->receiver;
    delete this->buffer;

    this->receiver = NULL;
    this->buffer   = NULL;
    DeleteCriticalSection(&this->lockMutex);
    LOGI("[%s:%d]doing successful! \n",__FUNCTION__,__LINE__);
}

bool        DASHManager2::Start                  (bool abDump)
{
    if (!this->receiver->Start())
        return false;

    this->isDumping       = true;
    if(abDump)
    {
    	this->dumpingThread   = CreateThreadPortable (DoDumping, this);
    	if(this->dumpingThread == NULL)
    	{
        	this->isDumping = false;
        	return false;
    	}
    }
    
    this->isRunning = true;
    LOGI("[%s:%d]doing successful! \n",__FUNCTION__,__LINE__);
    return true;
}
void        DASHManager2::Stop                   ()
{
    if (!this->isRunning)
        return;

    this->isDumping = false;
    if(this->dumpingThread != NULL)
    {
        JoinThread(this->dumpingThread);
        DestroyThreadPortable(this->dumpingThread);
    }

    this->isRunning = false;
    if(this->currentMediaDecoder)
    	delete this->currentMediaDecoder;
    this->currentMediaDecoder= NULL;

    this->receiver->Stop();
    this->buffer->Clear();
    this->readSegmentCount = 0;
    this->outputSegmentCount = 0;
    LOGI("[%s:%d]doing successful! \n",__FUNCTION__,__LINE__);
}
uint32_t    DASHManager2::GetPosition            ()
{
    return this->receiver->GetPosition();
}
void        DASHManager2::SetPosition            (uint32_t segmentNumber)
{
    this->receiver->SetPosition(segmentNumber);
}
void        DASHManager2::SetPositionInMsec      (uint32_t milliSecs)
{
    this->receiver->SetPositionInMsecs(milliSecs);
}
int 	      DASHManager2::GetSegmentDuration()
{
    return this->receiver->GetSegmentDuration();
}
int 		DASHManager2::GetSegmentTimescale(){
	return this->receiver->GetSegmentTimescale();
}
int 	      DASHManager2::GetSize()
{
    return this->receiver->GetSize();
}
int32_t    DASHManager2::GetSessionType()
{
    return session_type;
}
void        DASHManager2::Clear                  ()
{
    this->buffer->Clear();
}
void        DASHManager2::ClearTail              ()
{
    this->buffer->ClearTail();
}

// bandwidth change or dynamic refresh.
void        DASHManager2::SetRepresentation      (IMPD* mpd, IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation)
{
    EnterCriticalSection(&this->lockMutex);
    if (adaptationSet) {
        mAdaptation = adaptationSet;
        this->ConstructRepresentationMap();
    }
    IRepresentation * rep = representation;
    std::map<int64_t, IRepresentation *>::reverse_iterator rit;
    for (rit = mRep.rbegin(); rit != mRep.rend(); ++rit) {
        if (rit->first <= mBandwidth) {
            rep = rit->second;
            break;
        }
    }
    if (rit == mRep.rend()) {
        rep = mRep.begin()->second;
    }
    LOGI("[%s:%d] download bandwidth : %lld bps, representation bandwidth : %lld bps\n", __FUNCTION__, __LINE__, mBandwidth, (int64_t)(rep->GetBandwidth()));
    this->receiver->SetRepresentation(mpd, period, adaptationSet, rep);
    LeaveCriticalSection(&this->lockMutex);
}

void    DASHManager2::ConstructRepresentationMap()
{
    mRep.clear();
    IRepresentation * rep;

    int32_t i, j = 0;
    bool tricky = true;
    for (i = 0; i < mAdaptation->GetRepresentation().size(); i++) {
        rep = mAdaptation->GetRepresentation().at(i);
        std::vector<ISubRepresentation *> sub_rep = rep->GetSubRepresentations();
        if (sub_rep.size() > 1) {
            j++;
        }
    }

    if (!j || j == i) {
        tricky = false;
    }

    for (i = 0; i < mAdaptation->GetRepresentation().size(); i++) {
        rep = mAdaptation->GetRepresentation().at(i);
        std::vector<ISubRepresentation *> sub_rep = rep->GetSubRepresentations();
        if (sub_rep.size() > 1 && tricky) {
            continue;
        }
        mRep.insert(pair<int64_t, IRepresentation *>((int64_t)(rep->GetBandwidth()), rep));
    }
}

void        DASHManager2::OnBufferStateChanged   (uint32_t fillstateInPercent)
{

}
void        DASHManager2::OnSegmentDownloaded    ()
{
    this->readSegmentCount++;

    // notify observers
}

void    DASHManager2::OnEstimatedBandwidthBps(int64_t bandwidth)
{
    LOGI("[%s:%d] bandwidth notified, select suitable representation!\n",__FUNCTION__,__LINE__);
    mBandwidth = bandwidth;
    if (!isDynamic) { // SetRepresentation in static mode only, prevent run into chaos.
        SetRepresentation(NULL,NULL,NULL,NULL);
    }
}

int 		DASHManager2::Read (uint8_t *data, size_t len, int *flags)
{
    if (!this->isRunning || NULL == this->buffer){
	LOGE("[%s:%d] para is valid\n",__FUNCTION__,__LINE__);
       return -1;	
    }
    int ret = 0;

    MediaObject *initSegment = NULL;
    MediaObject *mediaSegment = NULL;
    while(this->isRunning){
    	if(url_irq_cb != NULL){
		if(url_irq_cb()>0){//seek break loop,not error response.
			LOGE("[%s:%d]url_irq_cb need exit! \n",__FUNCTION__,__LINE__);
			return DASH_SESSION_EXIT;
    		}    	
    	}
	if(NULL == this->currentMediaDecoder){
		if(*flags == 0)
			return ret;
		
		mediaSegment = this->buffer->GetFront();
		if(NULL == mediaSegment)	{
			if(this->buffer->GetEOS()){	// to the end
				LOGE("[%s:%d]read to the end!!!SegmentCount=%d, outputSegmentCount=%d\n",__FUNCTION__,__LINE__, this->readSegmentCount, this->outputSegmentCount);
				return DASH_SESSION_EOF;			// eof
			}

			// buffer has no data, need waiting
			PortableSleepMs(100);
			continue;
		}

		initSegment = this->receiver->FindInitSegment(mediaSegment->GetRepresentation());
		this->currentMediaDecoder = new MediaObjectDecoder(initSegment, mediaSegment);
		if(NULL == this->currentMediaDecoder){
			delete mediaSegment;
			mediaSegment=NULL;
			LOGE("[%s:%d]currentMediaDecoder new failed\n",__FUNCTION__,__LINE__);
			return -1;
		}
		this->outputSegmentCount++;
		LOGI("[%s:%d]new segment!!!, outputCount=%d, segmentNum=%d,initSegment=%x, mediaSegment=%x  \n",
				__FUNCTION__,__LINE__, this->outputSegmentCount,mediaSegment->GetSegmentNum(),initSegment,mediaSegment);
		
		initSegment = NULL;
		mediaSegment = NULL;
		*flags = 0;
	}  

	ret =  this->currentMediaDecoder->Read(data, len);
	if(ret > 0)
		break;

	LOGI("[%s:%d]out put segment to the End. cnt=%d \n",__FUNCTION__,__LINE__, this->outputSegmentCount);
	// ret=0 need get the front of the buffer to read data
	delete  this->currentMediaDecoder;
	this->currentMediaDecoder= NULL;
    }

    return ret;
}

void*		DASHManager2::DoDumping(void *dashmanager)
{
	cout<<" do dumping!!! " << std::endl;
	DASHManager2 *lpDashManager = (DASHManager2 *)dashmanager;
	if(NULL == lpDashManager)
		return NULL;

	MediaObject *initSegment = NULL;
    	MediaObject *mediaSegment = NULL;
	MediaObjectDecoder *mediaDecoder = NULL;
	FILE * lpReadFile= NULL;
	uint8_t lpBuffer[8192] = {0};
	char lpFileName[1024] = {0};
	int liFileCnt = 0;
	int liReadSize =0;
	int liReadSizeSum =0;
	while(lpDashManager->isDumping){
		if(NULL == mediaDecoder){			
			mediaSegment = lpDashManager->buffer->GetFront();
			if(NULL == mediaSegment){
				if(lpDashManager->buffer->GetEOS()){
					cout << " read to end!!! " << std::endl;
					break;
				}
				PortableSleepMs(100);
				continue;
			}

			initSegment = lpDashManager->receiver->FindInitSegment(mediaSegment->GetRepresentation());
			mediaDecoder = new MediaObjectDecoder(initSegment, mediaSegment);
			if(NULL == mediaDecoder){
				delete mediaSegment;
				mediaSegment= NULL;
				break;
			}

			liFileCnt++;
			sprintf(lpFileName, "%s/File%d.mp4", lpDashManager->dumpPath, liFileCnt);
			lpReadFile = fopen(lpFileName, "wb");
			cout<<" writing " <<lpFileName<< std::endl;
			liReadSizeSum = 0;
		}
		
		liReadSize = mediaDecoder->Read(lpBuffer, sizeof(lpBuffer));
		
		if(liReadSize > 0){
			fwrite(lpBuffer,1, liReadSize, lpReadFile);
			liReadSizeSum += liReadSize;
		}
		else{
			fclose(lpReadFile);
			lpReadFile = NULL;

			delete mediaDecoder;
			mediaDecoder= NULL;
			cout<<" read to end liReadSizeSum =" << liReadSizeSum << std::endl;
		}
	}

	if(mediaDecoder != NULL)
		delete mediaDecoder;

	cout<<" dumping over!!! " << std::endl;
	return NULL;
}
