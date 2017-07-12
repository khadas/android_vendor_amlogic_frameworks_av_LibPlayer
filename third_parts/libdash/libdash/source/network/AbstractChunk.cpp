/*
 * AbstractChunk.cpp
 *****************************************************************************
 * Copyright (C) 2012, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "AbstractChunk.h"
#include "libdash_extern_api.h"

using namespace dash::network;
using namespace dash::helpers;
using namespace dash::metrics;

uint32_t AbstractChunk::BLOCKSIZE = 32768;
uint32_t AbstractChunk::BANDWIDTH_NOTIFY_THRESHOLD = 1500;
uint64_t AbstractChunk::MAX_BLOCKSIZE = 1024*1024*20;
uint64_t AbstractChunk::MAX_BLOCKSIZE2 = 1024*1024*30;
uint32_t AbstractChunk::MAX_SLEEPTIME = 10000 ;   //  usec 


AbstractChunk::AbstractChunk        ()  :
               connection           (NULL),
               dlThread             (NULL),
               curl_handle (NULL),
               bytesDownloaded      (0),
               download_speed (0),
               close_flag (false)
{
}
AbstractChunk::~AbstractChunk       ()
{
    int32_t i = 0;
    this->close_flag = true;
    for (i = 0; i < tcpConnections.size(); i++) {
        delete tcpConnections.at(i);
    }

    for (i = 0; i < httpTransactions.size(); i++) {
        delete httpTransactions.at(i);
    }

    this->AbortDownload();
    this->blockStream.Clear();
    if (this->dlThread) {
        JoinThread(this->dlThread);
        DestroyThreadPortable(this->dlThread);
    }
    if (this->curl_handle) {
        curl_fetch_close(this->curl_handle);
    }
}

void    AbstractChunk::AbortDownload                ()
{
    this->stateManager.CheckAndSet(IN_PROGRESS, REQUEST_ABORT);
    this->stateManager.CheckAndWait(REQUEST_ABORT, ABORTED);
}
bool    AbstractChunk::StartDownload                ()
{
    if (this->stateManager.State() != NOT_STARTED
        && this->stateManager.State() != FAIL) { // for retry
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    char headers[4096] = {0};
    if(this->HasByteRange()) {
        snprintf(headers, sizeof(headers), "Range: bytes=%s\r\n", this->Range().c_str());
    }
    this->curl_handle = curl_fetch_init(this->AbsoluteURI().c_str(), headers, 0);
    if (!this->curl_handle) {
        goto fail;
    }
    curl_fetch_register_interrupt(this->curl_handle, extern_interrupt_cb);
    if (curl_fetch_open(this->curl_handle)) {
        LOGE("[%s:%d] curl fetch open failed!\n",__FUNCTION__,__LINE__);
        curl_fetch_close(this->curl_handle);
        this->curl_handle = NULL;
        goto fail;
    }

    LOGI("[%s:%d] url =%s", 
				__FUNCTION__, __LINE__, this->AbsoluteURI().c_str());
    this->dlThread = CreateThreadPortable (DownloadInternalConnection, this);

    if(this->dlThread == NULL) {
        curl_fetch_close(this->curl_handle);
        this->curl_handle = NULL;
        goto fail;
    }

    this->stateManager.State(IN_PROGRESS);

    return true;

fail:
    curl_global_cleanup();
    this->stateManager.State(FAIL);
    return false;
}
bool    AbstractChunk::StartDownload                (IConnection *connection)
{
    if(this->stateManager.State() != NOT_STARTED)
        return false;

    this->dlThread = CreateThreadPortable (DownloadExternalConnection, this);

    if(this->dlThread == NULL)
        return false;

    this->stateManager.State(IN_PROGRESS);
    this->connection = connection;

    return true;
}
int     AbstractChunk::Read                         (uint8_t *data, size_t len)
{
    return this->blockStream.GetBytes(data, len);
}
int     AbstractChunk::Peek                         (uint8_t *data, size_t len)
{
    return this->blockStream.PeekBytes(data, len);
}
int     AbstractChunk::Peek                         (uint8_t *data, size_t len, size_t offset)
{
    return this->blockStream.PeekBytes(data, len, offset);
}
void    AbstractChunk::AttachDownloadObserver       (IDownloadObserver *observer)
{
    this->observers.push_back(observer);
    this->stateManager.Attach(observer);
}
void    AbstractChunk::DetachDownloadObserver       (IDownloadObserver *observer)
{
    uint32_t pos = -1;

    for(size_t i = 0; i < this->observers.size(); i++)
        if(this->observers.at(i) == observer)
            pos = i;

    if(pos != -1)
        this->observers.erase(this->observers.begin() + pos);

    this->stateManager.Detach(observer);
}
void*   AbstractChunk::DownloadExternalConnection   (void *abstractchunk)
{
    AbstractChunk   *chunk  = (AbstractChunk *) abstractchunk;
    block_t         *block  = AllocBlock(chunk->BLOCKSIZE);
    int             ret     = 0;

    do
    {
	 if(chunk->blockStream.Length() > MAX_BLOCKSIZE){
		usleep(MAX_SLEEPTIME);
		ret=1;
		continue;
	 }
        ret = chunk->connection->Read(block->data, block->len, chunk);
        if(ret > 0)
        {
            block_t *streamblock = AllocBlock(ret);
            memcpy(streamblock->data, block->data, ret);
            chunk->blockStream.PushBack(streamblock);
            chunk->bytesDownloaded += ret;

            chunk->NotifyDownloadRateChanged();
        }
        if(chunk->stateManager.State() == REQUEST_ABORT)
            ret = 0;

    }while(ret);

    DeleteBlock(block);

    if(chunk->stateManager.State() == REQUEST_ABORT)
        chunk->stateManager.State(ABORTED);
    else
        chunk->stateManager.State(COMPLETED);

    chunk->blockStream.SetEOS(true);

    return NULL;
}
void*   AbstractChunk::DownloadInternalConnection   (void *abstractchunk)
{
    AbstractChunk *chunk  = (AbstractChunk *) abstractchunk;

    block_t * block = AllocBlock(chunk->BLOCKSIZE);
    int ret = 0;
    while (!chunk->close_flag) {
        if (extern_interrupt_cb()) {
            LOGE("[%s:%d] download interrupt!\n", __FUNCTION__, __LINE__);
            break;
        }
        if(chunk->stateManager.State() == REQUEST_ABORT) {
            break;
        }
        uint64_t data_len = chunk->blockStream.Length();
        if (data_len > MAX_BLOCKSIZE2) {
            usleep(MAX_SLEEPTIME);
            continue;
        }
        ret = curl_fetch_read(chunk->curl_handle, (char *)block->data, block->len);
        if (ret > 0) {
            block_t * streamblock = AllocBlock(ret);
            memcpy(streamblock->data, block->data, ret);
            chunk->blockStream.PushBack(streamblock);
            chunk->bytesDownloaded += ret;
            chunk->NotifyDownloadRateChanged();
        } else if (ret == C_ERROR_EAGAIN) {
            usleep(MAX_SLEEPTIME);
        } else {
            LOGI("[%s:%d] curl download ret : %d !\n", __FUNCTION__, __LINE__, ret);
            break;
        }
    }

    DeleteBlock(block);

    double tmp_info = 0.0;
    ret = curl_fetch_get_info(chunk->curl_handle, C_INFO_SPEED_DOWNLOAD, 0, (void *)&tmp_info);
    if (!ret) {
        chunk->download_speed = (int64_t)(tmp_info * 8);
    } else {
        chunk->download_speed = 0;
    }
    LOGI("[%s:%d] download speed : %lld bps \n", __FUNCTION__, __LINE__, chunk->download_speed);

    // notify bandwidth in large chunk only, prevent measurement spiked.
    if (chunk->bytesDownloaded > chunk->BANDWIDTH_NOTIFY_THRESHOLD) {
        chunk->NotifyDownloadBandwidth();
    }

    curl_global_cleanup();

    if(chunk->stateManager.State() == REQUEST_ABORT)
        chunk->stateManager.State(ABORTED);
    else
        chunk->stateManager.State(COMPLETED);

    chunk->blockStream.SetEOS(true);

    return NULL;
}
void    AbstractChunk::NotifyDownloadRateChanged    ()
{
    for(size_t i = 0; i < this->observers.size(); i++)
        this->observers.at(i)->OnDownloadRateChanged(this->bytesDownloaded);
}

void    AbstractChunk::NotifyDownloadBandwidth()
{
#if 0
    int32_t fast_bw, mid_bw, avg_bw, calc_bw;
    bandwidth_measure_get_bandwidth(this->measure_handle, &fast_bw, &mid_bw, &avg_bw);
    calc_bw = fast_bw * 0.8 + mid_bw * 0.2;
    for(size_t i = 0; i < this->observers.size(); i++) {
        this->observers.at(i)->OnDownloadBandwidthBps((int64_t)calc_bw);
    }
#endif
    for(size_t i = 0; i < this->observers.size(); i++) {
        this->observers.at(i)->OnDownloadBandwidthBps(this->download_speed);
    }
}

size_t  AbstractChunk::CurlResponseCallback         (void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    AbstractChunk *chunk = (AbstractChunk *)userp;

    if(chunk->stateManager.State() == REQUEST_ABORT)
        return 0;


    uint64_t data_len=chunk->blockStream.Length();
    if(data_len > MAX_BLOCKSIZE2){
	usleep(MAX_SLEEPTIME);
	usleep(MAX_SLEEPTIME);
    } 
    else if(data_len > MAX_BLOCKSIZE){
	usleep(MAX_SLEEPTIME);
    } 

    block_t *block = AllocBlock(realsize);

    memcpy(block->data, contents, realsize);
    chunk->blockStream.PushBack(block);

    chunk->bytesDownloaded += realsize;
    chunk->NotifyDownloadRateChanged();

    return realsize;
}
size_t  AbstractChunk::CurlDebugCallback            (CURL *url, curl_infotype infoType, char * data, size_t length, void *userdata)
{
    AbstractChunk   *chunk      = (AbstractChunk *)userdata;

    switch (infoType) {
        case CURLINFO_TEXT:
            break;
        case CURLINFO_HEADER_OUT:
            chunk->HandleHeaderOutCallback();
            break;
        case CURLINFO_HEADER_IN:
            chunk->HandleHeaderInCallback(std::string(data));
            break;
        case CURLINFO_DATA_IN:
            break;
        default:
            return 0;
    }
    return 0;
}
void    AbstractChunk::HandleHeaderOutCallback      ()
{
    HTTPTransaction *httpTransaction = new HTTPTransaction();

    httpTransaction->SetOriginalUrl(this->AbsoluteURI());
    httpTransaction->SetRange(this->Range());
    httpTransaction->SetType(this->GetType());
    httpTransaction->SetRequestSentTime(Time::GetCurrentUTCTimeStr());

    this->httpTransactions.push_back(httpTransaction);
}
void    AbstractChunk::HandleHeaderInCallback       (std::string data)
{
    HTTPTransaction *httpTransaction = this->httpTransactions.at(this->httpTransactions.size()-1);

    if (data.substr(0,4) == "HTTP")
    {
        httpTransaction->SetResponseReceivedTime(Time::GetCurrentUTCTimeStr());
        httpTransaction->SetResponseCode(strtoul(data.substr(9,3).c_str(), NULL, 10));
    }

    httpTransaction->AddHTTPHeaderLine(data);
}
const std::vector<ITCPConnection *>&    AbstractChunk::GetTCPConnectionList    () const
{
    return (std::vector<ITCPConnection *> &) this->tcpConnections;
}
const std::vector<IHTTPTransaction *>&  AbstractChunk::GetHTTPTransactionList  () const
{
    return (std::vector<IHTTPTransaction *> &) this->httpTransactions;
}
