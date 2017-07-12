#include "libdash_wrapper.h"
#include "libdash_extern_api.h"
#include "DASHSession.h"
#include "libdash.h"
#include "IMPD.h"
#include "Input/DASHManager2.h"
#include "MPD/AdaptationSetHelper.h"
#include "dash_common.h"
#include <iostream>
#include <fstream>
#include<vector>

using namespace dash;
using namespace dash::network;
using namespace std;
using namespace dash::mpd;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

static std::string mLocation;

int64_t GetDuration(const char *strDuration){
	if(NULL == strDuration)
		return 0;
	
	if(strlen(strDuration) < 2)
		return 0;
	
	char lszDuration[300] = {0};
	char *lpH = NULL;
	char *lpM = NULL;
	char *lpS = NULL;
	int64_t i64Duration = 0;
	strncpy(lszDuration, strDuration + 2, 300);
	cout << " lszDuration =" << lszDuration << endl;
	lpH = lszDuration;
	char * lOffset = strchr(lpH, 'H');
	if(lOffset != NULL){
		*lOffset = '\0';
		i64Duration += atof(lpH) * 3600 * 1000;
		
		lOffset++;
		cout << " strtok(lOffset, H) " << lpH << " lOffset:" << lOffset << endl;
	}
	else
		lOffset = lpH;
		
	lpM = lOffset;
	lOffset = strchr(lpM, 'M');
	if(lOffset != NULL){
		*lOffset = '\0';
		i64Duration += atof(lpM) * 60 * 1000;
		
		lOffset++;
		cout << " strtok(lOffset, M) " << lpM << " lOffset:" << lOffset << endl;
	}
	else
		lOffset = lpM;

	lpS = lOffset;	
	lOffset = strchr(lpS, 'S');
	if(lOffset != NULL){
		*lOffset = '\0';
		i64Duration += atof(lpS) *  1000;
		cout << " strtok(lOffset, S) " << lpS << endl;
	}

	return i64Duration;
}	

int dash_probe(const char *url, URL_IRQ_CB cb){
	if(NULL == url)
		return -1;

       // set amplayer interrupt callback to libdash global interrupt.
       extern_set_interrupt_cb(cb);

	IMPD                  *mpd = NULL;
	IPeriod  *currentPeriod = NULL;
	IAdaptationSet * adaptationSet = NULL;
	IDASHManager    	*dashManager = CreateDashManager() ;
	if(NULL == dashManager){
		LOGE("[%s:%d] CreateDashManager failed", __FUNCTION__, __LINE__);
		return -1;
	}

	int MimeType = -1;
	do
	{
		mpd = dashManager->Open((char *)url);
		if(NULL == mpd){
			LOGE("[%s:%d] Mdp open failed url=%s", __FUNCTION__, __LINE__, url);
			break;
		}

              mLocation.clear();
              if (!mpd->GetLocations().empty()) {
		    mLocation = mpd->GetLocations().at(0);
              }

		currentPeriod= mpd->GetPeriods().at(0);   
		if(NULL == currentPeriod){
			LOGE("[%s:%d] have no period!", __FUNCTION__, __LINE__);
			break;	
		}

		adaptationSet = currentPeriod->GetAdaptationSets().at(0);
		if(NULL == adaptationSet){
			LOGE("[%s:%d] have no adaptationSet!", __FUNCTION__, __LINE__);
			break;	
		}

		if(AdaptationSetHelper::IsContainedInMimeType(adaptationSet, "mp2t")){
			MimeType = DASH_SFORMAT_MP2T;
			LOGI("[%s:%d] MimeType is mp2t!", __FUNCTION__, __LINE__);
		}
		else if(AdaptationSetHelper::IsContainedInMimeType(adaptationSet, "mp4")){
			MimeType = DASH_SFORMAT_MP4;
			LOGI("[%s:%d] MimeType is mp4!", __FUNCTION__, __LINE__);
		}	
		else{
			MimeType = DASH_SFORMAT_NO;
			LOGI("[%s:%d] MimeType is unknow type!", __FUNCTION__, __LINE__);
		}
	}while(false);
	if (mpd) {
	    mpd->Delete();
	}
	if(dashManager != NULL)
		dashManager->Delete();
	return MimeType;
}
int dash_open(DASH_CONTEXT *dashCtx, const char *url, URL_IRQ_CB cb){
	if(NULL == url || NULL == dashCtx)
		return -1;

       // set amplayer interrupt callback to libdash global interrupt.
       extern_set_interrupt_cb(cb);

	IMPD                  *mpd = NULL;
	IDASHManager    	*dashManager = CreateDashManager() ;
	if(NULL == dashManager){
		LOGE("[%s:%d] CreateDashManager failed", __FUNCTION__, __LINE__);
		return -1;
	}
	dashCtx->dashmanager = (void *)dashManager;

	const char * realURL = NULL;
	if (mLocation.empty()) {
	    realURL = url;
	} else {
	    realURL = mLocation.c_str();
	}

       int sessionType;
       uint32_t sessionNum;
	DASHSession * session = new DASHSession(realURL, cb);
	int ret = session->Open();
	if (ret) {
	    LOGE("[%s:%d] DASHSession open failed!\n", __FUNCTION__, __LINE__);
	    goto FAIL;
	}
	sessionNum = session->getSessionNumber();
	if (sessionNum == 1) {
	    sessionType = session->getSessionType();
	    dashCtx->sessions[0].dashsession = (void *)session;
           dashCtx->sessions[0].session_type = sessionType;
	} else if (sessionNum == 2) {
	    dashCtx->sessions[0].dashsession = (void *)session;
	    dashCtx->sessions[1].dashsession = (void *)session;
	    dashCtx->sessions[0].session_type = DASH_STYPE_AUDIO;
	    dashCtx->sessions[1].session_type = DASH_STYPE_VIDEO;
	} else {
	    LOGE("[%s:%d] invalid session number!\n", __FUNCTION__, __LINE__);
	    goto FAIL;
	}
	dashCtx->durationUs = session->getDurationUs();
	dashCtx->nb_session = session->getSessionNumber();
	return ret;

FAIL:
	dash_close(dashCtx);
	return -1;
}

int dash_seek(DASH_CONTEXT *dashCtx, int64_t pos){
	if(NULL == dashCtx){
		LOGE("[%s:%d] dashCtx is null", __FUNCTION__, __LINE__);
		return -1;
	}

	if(dashCtx->durationUs<=0 || pos<0 || (pos*1000) > dashCtx->durationUs){
		LOGE("[%s:%d] pos %d is wrong ", __FUNCTION__, __LINE__, pos);
		return -1;
	}

	DASHSession * session = (DASHSession *)dashCtx->sessions[0].dashsession;
	if (!session) {
	    LOGE("[%s:%d] session is null!\n", __FUNCTION__, __LINE__);
	    return -1;
	}
	int seekTime = session->SeekTo(pos);
	dashCtx->sessions[0].seekStartTime = session->getSeekStartTime(dashCtx->sessions[0].session_type);
	if(dashCtx->nb_session == 2) {
	    dashCtx->sessions[1].seekStartTime = session->getSeekStartTime(dashCtx->sessions[1].session_type);
	}

	return seekTime;
}

int dash_read(DASH_SESSION_CONTEXT *sessionCtx,  uint8_t *data, size_t len){
	if(NULL == sessionCtx){
		LOGE("[%s:%d] sessionCtx is null", __FUNCTION__, __LINE__);
		return -1;
	}

	DASHSession * session = (DASHSession *)sessionCtx->dashsession;
	if (!session) {
	    LOGE("[%s:%d] session is null!\n", __FUNCTION__, __LINE__);
	    return -1;
	}
	int ret = session->Read(data, len, &(sessionCtx->flags), sessionCtx->session_type);

	return ret;
}

void dash_close(DASH_CONTEXT *dashCtx){
	if(NULL == dashCtx)
		return;

       LOGI("[%s:%d] dash close enter!\n", __FUNCTION__, __LINE__);
       DASHSession * session = (DASHSession *)dashCtx->sessions[0].dashsession; // just first.
       if (session) {
           delete session;
       }
	dashCtx->nb_session = 0;

	IDASHManager *dashManager = (IDASHManager *)dashCtx->dashmanager;
	if(dashManager != NULL)
		dashManager->Delete();
	dashCtx->dashmanager = NULL;
}
