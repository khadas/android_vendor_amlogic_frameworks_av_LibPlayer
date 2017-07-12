/*
 * DASHSession.h
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef LIBDASH_FRAMEWORK_DASHSESSION_H_
#define LIBDASH_FRAMEWORK_DASHSESSION_H_

#include "IDASHManager.h"
#include "Input/DASHManager2.h"
#include "Portable/MultiThreading.h"
#include "MPD/AdaptationSetHelper.h"
#include "dash_common.h"
#include "libdash.h"

using namespace libdash::framework::input;
using namespace libdash::framework::buffer;
using namespace libdash::framework::mpd;
using namespace libdash::framework;

using namespace dash;
using namespace dash::network;
using namespace dash::mpd;
using namespace std;


namespace libdash
{
    namespace framework
    {
        class DASHSession
        {
            public:
                DASHSession(const char *url, URL_IRQ_CB irq);
                virtual ~DASHSession();

                /////////// api to fetch session info ////////////
                int64_t getDurationUs();
                uint32_t getSessionNumber();
                int32_t getSessionType();
                int64_t getSeekStartTime(int32_t session_type);
                ///////////////////////////////////////////

                int32_t Open();
                int32_t Read(uint8_t *data, int32_t len, int32_t * flags, int32_t session_index);
                int64_t SeekTo(int64_t pos);

            private:
                int32_t init_dash_session();
                int32_t create_session_manager(IPeriod  *period, IAdaptationSet * adaptationSet, int32_t session_type);
                int32_t choose_best_bandwidth();
                int32_t refresh_dash_session();
                DASHManager2 * get_audio_session_manager();
                DASHManager2 * get_video_session_manager();
                static void * Session_Worker(void * session);
                void session_wait_timeUs(uint32_t microseconds);
                void Close();
                void DumpInfo();


                std::map<int32_t, int64_t> mSeekStartTime;
                CRITICAL_SECTION    sessionMutex;
                CONDITION_VARIABLE    sessionCond;
                THREAD_HANDLE    workerThread;
                IDASHManager    *mManager;
                vector<DASHManager2 *>    mManager2;
                IMPD    *mMPD;
                const char * mURL;
                URL_IRQ_CB	    irq_cb;

                uint32_t    mCheckTime;
                int64_t    mDurationUs;
                int32_t    mSessionType;
                bool    mIsDynamic;
                bool    mClose;
        };
    }
}

#endif