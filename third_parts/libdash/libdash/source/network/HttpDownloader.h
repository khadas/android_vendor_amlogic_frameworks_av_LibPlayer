/*
 * HttpDownloader.h
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#ifndef HTTPDOWNLOADER_H_
#define HTTPDOWNLOADER_H_

#include "config.h"

#include "../network/AbstractChunk.h"
#include "../helpers/Path.h"
#include "IBaseUrl.h"
#include "../metrics/HTTPTransaction.h"

namespace dash
{
    namespace network
    {
        class HttpDownloader : public network::AbstractChunk, public dash::network::IDownloadObserver
        {
            public:
                HttpDownloader         ();
                virtual ~HttpDownloader();

                bool                                Init            (const std::string &url,
                                                                     const std::string &range, dash::metrics::HTTPTransactionType type);
                std::string&                        AbsoluteURI     ();
                std::string&                        Host            ();
                size_t                              Port            ();
                std::string&                        Path            ();
                std::string&                        Range           ();
                size_t                              StartByte       ();
                size_t                              EndByte         ();
                bool                                HasByteRange    ();
                dash::metrics::HTTPTransactionType  GetType         ();

				// return -1 means eof;
				virtual int     Read                    (uint8_t *data, size_t len);

				virtual void    OnDownloadStateChanged  (dash::network::DownloadState state);
                virtual void    OnDownloadRateChanged   (uint64_t bytesDownloaded);
                virtual void    OnDownloadBandwidthBps (int64_t bandwidth);
            
			private:
				dash::network::DownloadState GetState();


                std::string                         absoluteuri;
                std::string                         host;
                size_t                              port;
                std::string                         path;
                std::string                         range;
                size_t                              startByte;
                size_t                              endByte;
                bool                                hasByteRange;
                dash::metrics::HTTPTransactionType  type;

				mutable CRITICAL_SECTION    stateLock;
				dash::network::DownloadState    state;
        };
    }
}

#endif /* HTTPDOWNLOADER_H_ */
