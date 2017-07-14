/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */


#include "hls_cache.h"

int main(int argc, char **argv)
{
    unsigned int size = 10 * 1024 * 1024;
    enum hls_cache_mode mode = HLS_CACHE_MODE_ITEM;
    struct hls_cache *cache = hls_cache_create(size, mode);
    hls_cache_release(cache);
    return 0;
}
