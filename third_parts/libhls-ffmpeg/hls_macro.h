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



#ifndef HLS_MACRO_H
#define HLS_MACRO_H

#define INITIAL_BUFFER_SIZE 32768

#define MAX_FIELD_LEN 64
#define MAX_CHARACTERISTICS_LEN 512

#define MPEG_TIME_BASE 90000
#define MPEG_TIME_BASE_Q (AVRational){1, MPEG_TIME_BASE}

// user defined
#define HLS_MAX_CACHE_SIZE 10*1024*1024
#define HLS_TIME_BASE 1000000
#define MAX_URL_SIZE 4096

#endif
