/*
 * Segment.cpp
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "HttpDownloader.h"

using namespace dash::network;
using namespace dash::helpers;
using namespace dash::metrics;

HttpDownloader::HttpDownloader    ()  :
            host(""),
            port(0),
            path(""),
            startByte(0),
            endByte(0),
            hasByteRange(false)
{
	InitializeCriticalSection   (&this->stateLock);
}
HttpDownloader::~HttpDownloader   ()
{
}

bool                HttpDownloader::Init               (const std::string &url, const std::string &range, HTTPTransactionType type)
{
    std::string host        = "";
    size_t      port        = 80;
    std::string path        = "";
    size_t      startByte   = 0;
    size_t      endByte     = 0;

    this->absoluteuri = url;

    if (dash::helpers::Path::GetHostPortAndPath(this->absoluteuri, host, port, path))
    {
        this->host = host;
        this->port = port;
        this->path = path;

        if (range != "" && dash::helpers::Path::GetStartAndEndBytes(range, startByte, endByte))
        {
            this->range         = range;
            this->hasByteRange  = true;
            this->startByte     = startByte;
            this->endByte       = endByte;
        }

        this->type = type;

		AbstractChunk::AttachDownloadObserver(this);
        return true;
    }

    return false;
}
std::string&        HttpDownloader::AbsoluteURI        ()
{
    return this->absoluteuri;
}
std::string&        HttpDownloader::Host               ()
{
    return this->host;
}
size_t              HttpDownloader::Port               ()
{
    return this->port;
}
std::string&        HttpDownloader::Path               ()
{
    return this->path;
}
std::string&        HttpDownloader::Range              ()
{
    return this->range;
}
size_t              HttpDownloader::StartByte          ()
{
    return this->startByte;
}
size_t              HttpDownloader::EndByte            ()
{
    return this->endByte;
}
bool                HttpDownloader::HasByteRange       ()
{
    return this->hasByteRange;
}
HTTPTransactionType HttpDownloader::GetType            ()
{
    return this->type;
}
int                 HttpDownloader::Read                   (uint8_t *data, size_t len)
{
	int ret = AbstractChunk::Read(data, len);

	if(ret <= 0)
	{
		DownloadState    state = this->GetState();
		if(this->state == COMPLETED || this->state != ABORTED)
			return -1;
	}
	return ret;
}
DownloadState HttpDownloader::GetState()
{
	DownloadState    tempstate;
	EnterCriticalSection(&this->stateLock);
	tempstate = this->state;
	LeaveCriticalSection(&this->stateLock);
	return tempstate;
}
void                HttpDownloader::OnDownloadStateChanged (DownloadState state)
{
	EnterCriticalSection(&this->stateLock);
    this->state = state;
	LeaveCriticalSection(&this->stateLock);
}
void                HttpDownloader::OnDownloadRateChanged  (uint64_t bytesDownloaded)
{
}

void    HttpDownloader::OnDownloadBandwidthBps(int64_t bandwidth)
{
}