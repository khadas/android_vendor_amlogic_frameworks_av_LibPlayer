/*
 * Copyright (C) 2010 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
