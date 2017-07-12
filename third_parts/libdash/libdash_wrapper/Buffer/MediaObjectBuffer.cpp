/*
 * MediaObjectBuffer.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "MediaObjectBuffer.h"

using namespace libdash::framework::buffer;
using namespace libdash::framework::input;

using namespace dash::mpd;
using namespace dash::network;
using namespace std;

//#define TRACE() LOGI("[%s:%d]Trace!!! \n",__FUNCTION__,__LINE__);
#define TRACE() 

MediaObjectBuffer::MediaObjectBuffer    (uint32_t maxcapacity) :
                   eos                  (false),
                   maxcapacity          (maxcapacity)
{
    InitializeConditionVariable (&this->full);
    InitializeConditionVariable (&this->empty);
    InitializeCriticalSection   (&this->monitorMutex);
}
MediaObjectBuffer::~MediaObjectBuffer   ()
{
    this->Clear();

    DeleteConditionVariable (&this->full);
    DeleteConditionVariable (&this->empty);
    DeleteCriticalSection   (&this->monitorMutex);
}

bool            MediaObjectBuffer::PushBack         (MediaObject *media)
{
    TRACE() 
    EnterCriticalSection(&this->monitorMutex);
    TRACE() 

    while(this->mediaobjects.size() >= this->maxcapacity && !this->eos)
        SleepConditionVariableCS(&this->empty, &this->monitorMutex, INFINITE);
    TRACE() 
    if(this->mediaobjects.size() >= this->maxcapacity)
    {
        LeaveCriticalSection(&this->monitorMutex);
    	 TRACE() 
        return false;
    }

    this->mediaobjects.push_back(media);

    WakeAllConditionVariable(&this->full);
    LeaveCriticalSection(&this->monitorMutex);
    this->Notify();
    TRACE() 
    return true;
}
MediaObject*    MediaObjectBuffer::Front            ()
{
    TRACE() 
    EnterCriticalSection(&this->monitorMutex);
    TRACE() 
    while(this->mediaobjects.size() == 0 && !this->eos)
        SleepConditionVariableCS(&this->full, &this->monitorMutex, INFINITE);
    TRACE() 
    if(this->mediaobjects.size() == 0)
    {
        LeaveCriticalSection(&this->monitorMutex);
        TRACE() 
        return NULL;
    }

    MediaObject *object = this->mediaobjects.front();

    LeaveCriticalSection(&this->monitorMutex);
    TRACE() 
    return object;
}
MediaObject*    MediaObjectBuffer::GetFront         ()
{
    TRACE() 
    EnterCriticalSection(&this->monitorMutex);
    TRACE() 
    //while(this->mediaobjects.size() == 0 && !this->eos)
    //    SleepConditionVariableCS(&this->full, &this->monitorMutex, INFINITE);
    TRACE() 
    if(this->mediaobjects.size() == 0)
    {
        LeaveCriticalSection(&this->monitorMutex);
        TRACE() 
        return NULL;
    }

    MediaObject *object = this->mediaobjects.front();
    this->mediaobjects.pop_front();

    WakeAllConditionVariable(&this->empty);
    LeaveCriticalSection(&this->monitorMutex);
    this->Notify();
	TRACE() 
    return object;
}
uint32_t        MediaObjectBuffer::Length           ()
{
    TRACE() 
    EnterCriticalSection(&this->monitorMutex);
    TRACE() 
    uint32_t ret = this->mediaobjects.size();
TRACE() 
    LeaveCriticalSection(&this->monitorMutex);
TRACE() 
    return ret;
}
void            MediaObjectBuffer::PopFront         ()
{
TRACE() 
    EnterCriticalSection(&this->monitorMutex);
TRACE() 
    this->mediaobjects.pop_front();

    WakeAllConditionVariable(&this->empty);
    LeaveCriticalSection(&this->monitorMutex);
    this->Notify();
    TRACE() 
}
void            MediaObjectBuffer::SetEOS           (bool value)
{
TRACE() 
    EnterCriticalSection(&this->monitorMutex);
TRACE() 
    for (size_t i = 0; i < this->mediaobjects.size(); i++)
        this->mediaobjects.at(i)->AbortDownload();

    this->eos = value;

    WakeAllConditionVariable(&this->empty);
    WakeAllConditionVariable(&this->full);
    LeaveCriticalSection(&this->monitorMutex);
    TRACE() 
}
bool            MediaObjectBuffer::GetEOS()
{
	bool lres = false;
	TRACE() 
	EnterCriticalSection(&this->monitorMutex);
	TRACE() 
	lres = this->eos;
	LeaveCriticalSection(&this->monitorMutex);
	TRACE() 
	return lres;
}
void            MediaObjectBuffer::AttachObserver   (IMediaObjectBufferObserver *observer)
{
    this->observer.push_back(observer);
}
void            MediaObjectBuffer::Notify           ()
{
    for(size_t i = 0; i < this->observer.size(); i++)
        this->observer.at(i)->OnBufferStateChanged((int)((double)this->mediaobjects.size()/(double)this->maxcapacity*100.0));
}
void            MediaObjectBuffer::ClearTail        ()
{
TRACE() 
    EnterCriticalSection(&this->monitorMutex);
TRACE() 
    int size = this->mediaobjects.size() - 1;

    if (size < 1)
    {
        LeaveCriticalSection(&this->monitorMutex);
        TRACE() 
        return;
    }

    MediaObject* object = this->mediaobjects.front();
    this->mediaobjects.pop_front();
    for(int i=0; i < size; i++)
    {
        delete this->mediaobjects.front();
        this->mediaobjects.pop_front();
    }

    this->mediaobjects.push_back(object);
    WakeAllConditionVariable(&this->empty);
    WakeAllConditionVariable(&this->full);
    LeaveCriticalSection(&this->monitorMutex);
    TRACE() 
    this->Notify();
}
void            MediaObjectBuffer::Clear            ()
{
TRACE() 
    EnterCriticalSection(&this->monitorMutex);
TRACE() 

    deque<MediaObject *>::iterator iter=this->mediaobjects.begin();
    for(; iter!=this->mediaobjects.end();iter++)
    {
    	if(*iter != NULL)
    	{
    		delete *iter;
    		*iter=NULL;
    	}
    }
    /*for(int i=0; i < this->mediaobjects.size(); i++)
    {
        delete this->mediaobjects.front();
    }*/
    this->mediaobjects.clear();
    LOGI("[%s:%d]the remain=%d \n",__FUNCTION__,__LINE__,this->mediaobjects.size());
    WakeAllConditionVariable(&this->empty);
    WakeAllConditionVariable(&this->full);
    LeaveCriticalSection(&this->monitorMutex);
    TRACE() 
    this->Notify();
}
uint32_t        MediaObjectBuffer::Capacity         ()
{
    return this->maxcapacity;
}
