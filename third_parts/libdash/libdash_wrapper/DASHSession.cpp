/*
 * DASHSession.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "DASHSession.h"
#include "libdash_wrapper.h"
#include "../libdash/source/helpers/Time.h"
#include <stdio.h>
#include<sys/time.h>

#ifndef DUMP_INFO_ENABLE
#define DUMP_INFO_ENABLE 1
#endif

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace libdash::framework::mpd;
using namespace libdash::framework;

using namespace dash;
using namespace dash::network;
using namespace dash::mpd;
using namespace dash::helpers;
using namespace std;

DASHSession::DASHSession(const char * url, URL_IRQ_CB irq) :
    mURL(url),
    irq_cb(irq),
    mManager(NULL),
    mMPD(NULL),
    workerThread(NULL),
    mCheckTime(-1),
    mDurationUs(-1),
    mSessionType(0),
    mIsDynamic(false),
    mClose(false)
{
    InitializeCriticalSection(&sessionMutex);
    InitializeConditionVariable(&sessionCond);
}

DASHSession::~DASHSession()
{
    Close();

    if (mManager) {
        delete mManager;
    }

    for (int32_t i = 0; i < mManager2.size(); i++) {
        delete mManager2.at(i);
    }

    // delete mpd in DASHReceiver.

    DeleteCriticalSection(&sessionMutex);
    DeleteConditionVariable(&sessionCond);
}

int32_t DASHSession::Open()
{
    int32_t ret = 0;
    ret = init_dash_session();
    if (ret) {
        LOGE("[%s:%d] Open failed!\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

void DASHSession::Close()
{
    mClose = true;
    if (workerThread) {
        EnterCriticalSection(&sessionMutex);
        WakeAllConditionVariable(&sessionCond);
        LeaveCriticalSection(&sessionMutex);
        JoinThread(workerThread);
        DestroyThreadPortable(workerThread);
    }
    LOGI("[%s:%d] dash session closed!\n", __FUNCTION__, __LINE__);
}

int64_t DASHSession::getDurationUs()
{
    return mDurationUs;
}

uint32_t DASHSession::getSessionNumber()
{
    return mManager2.size();
}

int32_t DASHSession::getSessionType()
{
    return mSessionType;
}

int64_t DASHSession::getSeekStartTime(int32_t session_type)
{
    if (mSeekStartTime.find(session_type) != mSeekStartTime.end()) {
        return mSeekStartTime.find(session_type)->second;
    }

    return -1;
}

int32_t DASHSession::Read(uint8_t * data, int32_t len, int32_t * flags, int32_t session_type)
{
    if (mManager2.empty()) {
        LOGE("[%s:%d] DASHManager2's vector is empty!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    int32_t ret = 0;
    DASHManager2 * mgr = NULL;
    for (int32_t i = 0; i < mManager2.size(); i++) {
        mgr = mManager2.at(i);
        if (mgr->GetSessionType() == session_type) {
            ret = mgr->Read(data, len, flags);
            break;
        }
    }

    return ret;
}

int64_t DASHSession::SeekTo(int64_t pos)
{
    if (mManager2.empty()) {
        LOGE("[%s:%d] DASHManager2's vector is empty!\n", __FUNCTION__, __LINE__);
        return DASH_SEEK_UNSUPPORT;
    }

    DASHManager2 * mgr = NULL;
    int32_t segmentNum = 0;
    int64_t seekTime = -1;
    int32_t i = 0;
    for (i = 0; i < mManager2.size(); i++) {
        mgr = mManager2.at(i);
        if (mgr->GetSize() <= 1) {
            segmentNum = 0;
        } else {
            segmentNum = pos * mgr->GetSegmentTimescale() / mgr->GetSegmentDuration();
        }
        mgr->Stop();
        mgr->SetPosition(segmentNum);
        mgr->Start(false);

        if (seekTime == -1 || mgr->GetSessionType() != DASH_STYPE_AUDIO) {
            seekTime = mgr->GetSegmentDuration() * segmentNum / mgr->GetSegmentTimescale();
        }
        int64_t seekStartTime = (int64_t)1000000 * (int64_t)mgr->GetSegmentDuration()*segmentNum / mgr->GetSegmentTimescale();
        mSeekStartTime[mgr->GetSessionType()] = seekStartTime;
        LOGI("[%s:%d] seek session : %d,  segment number : %d\n", __FUNCTION__, __LINE__, mgr->GetSessionType(), segmentNum);
    }
    LOGI("[%s:%d] seek to the time = %lld, pos = %lld\n", __FUNCTION__, __LINE__, seekTime, pos);
    return seekTime;
}

int32_t DASHSession::init_dash_session()
{
    if (!mURL) {
        LOGE("[%s:%d] url is invalid!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    mManager = CreateDashManager();
    if (!mManager) {
        LOGE("[%s:%d] CreateDashManager failed!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    mMPD = mManager->Open(mURL);
    if (!mMPD) {
        LOGE("[%s:%d] MPD open failed!\n", __FUNCTION__, __LINE__);
        return -1;
    }
    int32_t periodNum = mMPD->GetPeriods().size();
    if (periodNum <= 0) {
        LOGE("[%s:%d] no period!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    mIsDynamic = (mMPD->GetType() == "dynamic");
    mDurationUs = GetDuration(mMPD->GetMediaPresentationDuration().c_str());

    uint32_t fetchTime = mMPD->GetFetchTime();
    mCheckTime = fetchTime + TimeResolver::GetDurationInSec(mMPD->GetMinimumUpdatePeriod()) + 1; // round up

    // maybe need change when several periods are present, use first by default now.
    IPeriod  *currentPeriod = mMPD->GetPeriods().at(0);

    if (!mDurationUs) {
        mDurationUs = GetDuration(currentPeriod->GetDuration().c_str());
    }

    IAdaptationSet * adaptationSet = NULL;
    int32_t adaptationSetNum = currentPeriod->GetAdaptationSets().size();
    bool hasVideo = false, hasAudio = false;
    for (int32_t i = 0; i < adaptationSetNum && (!hasVideo || !hasAudio); i++) {
        adaptationSet = currentPeriod->GetAdaptationSets().at(i);
        if (!adaptationSet) {
            continue;
        }
        if (AdaptationSetHelper::IsAVAdaptationSet(adaptationSet)) {
            if (!create_session_manager(currentPeriod, adaptationSet, DASH_STYPE_AV)) {
                hasVideo = true;
                hasAudio = true;
                mSessionType |= DASH_STYPE_AV;
            }
        } else if (AdaptationSetHelper::IsAudioAdaptationSet(adaptationSet) && !hasAudio) {
            if (!create_session_manager(currentPeriod, adaptationSet, DASH_STYPE_AUDIO)) {
                hasAudio = true;
                mSessionType |= DASH_STYPE_AUDIO;
            }
        } else if (AdaptationSetHelper::IsVideoAdaptationSet(adaptationSet)) {
            if (!create_session_manager(currentPeriod, adaptationSet, DASH_STYPE_VIDEO)) {
                hasVideo = true;
                mSessionType |= DASH_STYPE_VIDEO;
            }
        }
    }

    if (!mManager2.size()) {
        return -1;
    }

#if DUMP_INFO_ENABLE
    DumpInfo();
#endif

    if (mIsDynamic) {
        workerThread = CreateThreadPortable(Session_Worker, this);
        if (!workerThread) {
            LOGE("[%s:%d] create worker thread failed!\n", __FUNCTION__, __LINE__);
            // no failure return here, just go on.
        }
    }

    return 0;
}

int32_t DASHSession::create_session_manager(IPeriod * period, IAdaptationSet * adaptationSet, int32_t session_type)
{
    DASHManager2 * mgr = NULL;

    // just temporary
    if (mIsDynamic) {
        mgr = new DASHManager2(5, mMPD, session_type, mIsDynamic, irq_cb);
    } else {
        mgr = new DASHManager2(2, mMPD, session_type, mIsDynamic, irq_cb);
    }
    mMPD->addRef(); // use Delete() corresponding when release mpd.
    LOGI("[%s:%d] manager2 is created for session type : %d \n", __FUNCTION__, __LINE__, session_type);

    int32_t representNum = adaptationSet->GetRepresentation().size();
    if (representNum <= 0) {
        LOGE("[%s:%d] no representation!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    /*************************/
    // TODO: add choose_best_bandwidth() here, to get proper representation.
    /*************************/

    IRepresentation *representation = adaptationSet->GetRepresentation().at(0);
    mgr->SetRepresentation(NULL, period, adaptationSet, representation);
    if (!mgr->Start(false)) { // segment download begin
        delete mgr;
        LOGE("[%s:%d] DASHManager2 Start Failed!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    mManager2.push_back(mgr);

    return 0;
}

int32_t DASHSession::choose_best_bandwidth()
{
    return 0;
}

DASHManager2 * DASHSession::get_audio_session_manager()
{
    for (vector<DASHManager2 *>::iterator it = mManager2.begin(); it != mManager2.end(); it++) {
        if ((*it)->GetSessionType() == DASH_STYPE_AUDIO) {
            return *it;
        }
    }
    return NULL;
}

DASHManager2 * DASHSession::get_video_session_manager()
{
    for (vector<DASHManager2 *>::iterator it = mManager2.begin(); it != mManager2.end(); it++) {
        if ((*it)->GetSessionType() == DASH_STYPE_VIDEO) {
            return *it;
        }
    }
    return NULL;
}

int32_t DASHSession::refresh_dash_session()
{
    mMPD = mManager->Open(mURL);
    if (!mMPD) {
        LOGE("[%s:%d] MPD open failed!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // TODO: add choose_best_bandwidth() here.

    IPeriod  *currentPeriod = mMPD->GetPeriods().at(0);
    IAdaptationSet * adaptationSet = NULL;
    DASHManager2 * tmpManager2 = NULL;
    int32_t adaptationSetNum = currentPeriod->GetAdaptationSets().size();
    bool hasVideo = false, hasAudio = false;
    for (int32_t i = 0; i < adaptationSetNum && (!hasVideo || !hasAudio); i++) {
        adaptationSet = currentPeriod->GetAdaptationSets().at(i);
        if (!adaptationSet) {
            continue;
        }
        if (AdaptationSetHelper::IsAVAdaptationSet(adaptationSet)) { // one manager
            mManager2.at(0)->SetRepresentation(mMPD, currentPeriod, adaptationSet, adaptationSet->GetRepresentation().at(0));
            hasVideo = true;
            hasAudio = true;
        } else if (AdaptationSetHelper::IsAudioAdaptationSet(adaptationSet) && !hasAudio) {
            tmpManager2 = get_audio_session_manager();
            if (tmpManager2) {
                tmpManager2->SetRepresentation(mMPD, currentPeriod, adaptationSet, adaptationSet->GetRepresentation().at(0));
                mMPD->addRef();
                hasAudio = true;
            }
        } else if (AdaptationSetHelper::IsVideoAdaptationSet(adaptationSet)) {
            tmpManager2 = get_video_session_manager();
            if (tmpManager2) {
                tmpManager2->SetRepresentation(mMPD, currentPeriod, adaptationSet, adaptationSet->GetRepresentation().at(0));
                mMPD->addRef();
                hasVideo = true;
            }
        }
    }

    if (!hasAudio && !hasVideo) {
        mMPD->Delete();
    }

    return 0;
}

void DASHSession::session_wait_timeUs(uint32_t microseconds)
{
    struct timeval now;
    struct timespec timeout;
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + (microseconds + now.tv_usec) / 1000000;
    timeout.tv_nsec = now.tv_usec * 1000;
    EnterCriticalSection(&sessionMutex);
    SleepConditionVariableTime(&sessionCond, &sessionMutex, &timeout);
    LeaveCriticalSection(&sessionMutex);
}

void * DASHSession::Session_Worker(void * session)
{
    DASHSession * dashSession = (DASHSession *)session;

    int32_t ret = 0;
    uint32_t updatePeriod = 0;
    string mini_up = dashSession->mMPD->GetMinimumUpdatePeriod();
    string suggestDelay = dashSession->mMPD->GetSuggestedPresentationDelay();
    bool constant_refresh = false;
    if (mini_up.empty() && !suggestDelay.empty()) {
        constant_refresh = true;
        updatePeriod = TimeResolver::GetDurationInSec(suggestDelay);
    }
    do {
        if (dashSession->mCheckTime > 0) {// first update
            uint32_t currTime = Time::GetCurrentUTCTimeInSec();
            if (currTime > dashSession->mCheckTime) {
                dashSession->session_wait_timeUs((currTime - dashSession->mCheckTime) * 1000000);
            }
            dashSession->mCheckTime = -1;
        }
        ret = dashSession->refresh_dash_session();
        if (ret) {
            LOGE("[%s:%d] refresh session failed!\n", __FUNCTION__, __LINE__);
            PortableSleepMs(100); // change to noblock, need to do.
        } else {
        #if DUMP_INFO_ENABLE
            dashSession->DumpInfo();
	 #endif
            if (!constant_refresh) {
                updatePeriod = TimeResolver::GetDurationInSec(dashSession->mMPD->GetMinimumUpdatePeriod()) + 1; //round up
            }
            LOGI("[%s:%d] session update period : %lu s\n", __FUNCTION__, __LINE__, updatePeriod);
            dashSession->session_wait_timeUs(updatePeriod * 1000000);
        }
    } while(!dashSession->mClose);

    LOGI("[%s:%d] worker end!\n", __FUNCTION__, __LINE__);
    return NULL;
}

void DASHSession::DumpInfo()
{
    LOGI("************************* Session info dump begin *************************\n");
    IPeriod * period;
    IAdaptationSet * adaptationset;
    IRepresentation *representation;
    vector<string> codecs;
    for (int32_t i = 0; i < mMPD->GetPeriods().size(); i++) { // more
        period = mMPD->GetPeriods().at(i);
        LOGI("period id : %s, start : %s, duration : %s \n", period->GetId().c_str(), period->GetStart().c_str(), period->GetDuration().c_str());
        for (int32_t j = 0; j < period->GetAdaptationSets().size(); j++) {
            adaptationset = period->GetAdaptationSets().at(j);
            LOGI("adaptationset id : %d, contentType : %s \n", adaptationset->GetId(), adaptationset->GetContentType().c_str());
            for (int32_t k = 0; k < adaptationset->GetRepresentation().size(); k++) {
                representation = adaptationset->GetRepresentation().at(k);
                LOGI("representation id : %s, mimeType : %s, bandwidth : %d \n", representation->GetId().c_str()
		  , representation->GetMimeType().c_str() ? representation->GetMimeType().c_str() : adaptationset->GetMimeType().c_str()
		  , representation->GetBandwidth());
                codecs = representation->GetCodecs();
                for (vector<string>::iterator it = codecs.begin(); it != codecs.end(); it++) {
                    if (it->find("avc") != std::string::npos) {
                        LOGI("codecType : %s, width : %d, height : %d, framerate : %s \n", it->c_str()
                        , representation->GetWidth() ? representation->GetWidth() : adaptationset->GetWidth()
                        , representation->GetHeight() ? representation->GetHeight() : adaptationset->GetHeight()
                        , representation->GetFrameRate().empty() ? adaptationset->GetFrameRate().c_str() : representation->GetFrameRate().c_str());
                    } else if (it->find("mp4a") != std::string::npos) {
                        LOGI("codecType : %s, samplerate : %s \n", it->c_str()
                        , representation->GetAudioSamplingRate().empty() ? adaptationset->GetAudioSamplingRate().c_str() : representation->GetAudioSamplingRate().c_str());
                    } else {
                        LOGI("codecType : %s \n", it->c_str());
                    }
                }
            }
        }
    }
    LOGI("Total session number : %d \n", mManager2.size());
    LOGI("************************* Session info dump end *************************\n");
}
