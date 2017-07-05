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




#ifndef ESPLAYER_H
#define ESPLAYER_H

/*
* =====================================================================================
* ESPlayer
* Description:
* =====================================================================================
*/

typedef enum MEDIA_CODEC_TYPE {
    DAL_ES_VCODEC_TYPE_H264,  /* video codec type */
    DAL_ES_ACODEC_TYPE_AAC, /* audio codec type */
    DAL_CODEC_TYPE_UNKNOWN,

} MEDIA_CODEC_TYPE_E;


/*
函数原型: void ES_PlayInit()
参数描述: 无
功能描述: 初始化ES播放器
返回值:  无
*/
void ES_PlayInit();

/*
函数原型: void ES_PlayDeinit()
参数描述: 无
功能描述:反初始化ES播放器，释放初始化ES播放器占用的相应资源
返回值: 无
*/
void ES_PlayDeinit();
/*
函数原型:void ES_PlayPause ()
参数描述:无
功能描述:暂停ES播放器播放
返回值:无
*/
void ES_PlayPause();
/*
函数原型:void ES_PlayResume ()
参数描述:无
功能描述:继续ES播放器播放
返回值:无
*/
void ES_PlayResume();
/*
函数原型
void ES_PlayFreeze ()
参数描述
无
功能描述
冻结ES播放音视频，解码器照常工作。
返回值
无
*/
void ES_PlayFreeze();

/*
函数原型: void ES_PlayResetESBuffer ()
参数描述:   无
功能描述:清空ES播放器buffer视音频数据
返回值:无
*/

void ES_PlayResetESBuffer();
/*
函数原型:void ES_PlayGetESBufferStatus (int *audio_rate,int *vid_rate)
参数描述:audio_rate:当前audio数据占buffer的百分比； vid_rate：当前video数据占buffer的百分比；
功能描述:获取audio/video所占buffer的百分比
返回值:    无
*/
void ES_PlayGetESBufferStatus(int *audio_rate, int *vid_rate);

/*
参数描述:    data_type：视音频数据编码类型； *es_buffer：视音频数据buffer； buffer_len：视音频数据buffer的长度（bytes）； PTS：解码器视音频所需要的PTS；
功能描述:将ES视音频数据推入ES播放器
返回值: 无
*/
void ES_PlayInjectionMediaDatas(MEDIA_CODEC_TYPE_E data_type, void *es_buffer, unsigned int buffer_len, unsigned int PTS);



#endif