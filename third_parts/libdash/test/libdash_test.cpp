/*
 * libdash_networkpart_test.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "IMPD.h"
#include "../libdash/include/libdash.h"
#include "../libdash_wrapper/Input/DASHManager2.h"
#include "../libdash_wrapper/MPD/AdaptationSetHelper.h"
#include "../libdash_wrapper/MPD/AdaptationSetHelper.h"
extern "C"{
#include "../libdash_wrapper/libdash_wrapper.h"	
}

#include <iostream>
#include <fstream>
using namespace dash;
using namespace dash::network;
using namespace std;
using namespace dash::mpd;
using namespace libdash::framework::input;
using namespace libdash::framework::mpd;

int main(int argc, char *argv[])
{
	IDASHManager    *manager  = CreateDashManager();
	if(NULL == manager){
		cout << " CreateDashManager Failed!" <<  endl;
		return 0;
	}

	char lszUrl[300] = "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mpeg2-simple-files/mpeg2-simple-files-mpd-template.mpd";
	IMPD                  *mpd = manager->Open(argv[1]);
	if(NULL == mpd){
		cout << " Mdp open " << lszUrl << " Failed!" <<  endl;
		return 0;
	}
	
	DASHManager2              *dashManager = new DASHManager2(2, mpd, 0, (mpd->GetType() == "dynamic"), NULL);
	if(NULL == dashManager){
		cout << " dashManager new Failed!" <<  endl;
		return 0;
	}
	
	IPeriod  *currentPeriod= mpd->GetPeriods().at(0);   
	IAdaptationSet * adaptationSet = NULL;
	int adaptationSetNum = 0;
	if(NULL == currentPeriod){
		cout << " have no period!" <<  endl;
		return 0;
	}

	adaptationSetNum = currentPeriod->GetAdaptationSets().size();
	for(int i = 0; i < adaptationSetNum; i++){
		adaptationSet = currentPeriod->GetAdaptationSets().at(i);
		if(NULL == adaptationSet)
			continue;
			
		if(AdaptationSetHelper::IsVideoAdaptationSet(adaptationSet))
			break;
		else
			adaptationSet = NULL;
	}
	
	if(NULL == adaptationSet){
		cout << " have no adaptation set!" <<  endl;
		return 0;
	} 
	
	dashManager->SetRepresentation(NULL, currentPeriod, adaptationSet, adaptationSet->GetRepresentation().at(0));
	
	if(!dashManager->Start(false)){
		cout << " dashManager Start Failed!" <<  endl;
		return 0;
	}

	char ldumpfile[300] = {0};
	sprintf(ldumpfile, "%s", argv[2]);

	
	std::cout<<" do reading!!! " << std::endl;

	uint8_t lpBuffer[8192] = {0};
	FILE *lhFile=fopen(ldumpfile, "wb");
	int ret = 0;
	int flag = 1;   
	while(true){
		ret = dashManager->Read(lpBuffer, sizeof(lpBuffer), &flag);
		if(ret <= 0)
			break;

		fwrite(lpBuffer, 1, ret, lhFile);
	}
	
	cout << " read to end!" <<  endl;
	delete dashManager;
	
   	return 0;
}
