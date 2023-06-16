/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install,
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2020, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef	HQUEUE_H
#define	HQUEUE_H

#ifdef __linux
    #define HT_API
#endif

#ifndef BOOL_DEF
typedef int				    BOOL;
#define BOOL_DEF
#endif

#include <stdint.h>

/***********************************************************/
#define	HQ_PUT_WAIT		0x00000001
#define	HQ_GET_WAIT		0x00000002
#define	HQ_NO_EVENT		0x00000004

/***********************************************************/
typedef struct h_queue
{
    uint32_t	queue_mode;
    uint32_t    unit_num;
    uint32_t    unit_size;
    uint32_t    front;
    uint32_t    rear;
    uint32_t    queue_buffer;
    uint32_t    count_put_full;

    void *      queue_putMutex;
    void *      queue_nnulEvent;
    void *      queue_nfulEvent;
} HQUEUE;


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************/
HT_API HQUEUE * hqCreate(uint32_t unit_num, uint32_t unit_size, uint32_t queue_mode);
HT_API void     hqDelete(HQUEUE * phq);

HT_API BOOL     hqBufPut(HQUEUE * phq,char * buf);
HT_API BOOL     hqBufGet(HQUEUE * phq,char * buf);

HT_API BOOL     hqBufIsEmpty(HQUEUE * phq);
HT_API BOOL     hqBufIsFull(HQUEUE * phq);

HT_API char   * hqBufGetWait(HQUEUE * phq);
HT_API void     hqBufGetWaitPost(HQUEUE * phq);

HT_API char   * hqBufPutPtrWait(HQUEUE * phq);
HT_API void     hqBufPutPtrWaitPost(HQUEUE * phq, BOOL bPutFinish);
HT_API BOOL     hqBufPeek(HQUEUE * phq,char * buf);

HT_API uint32_t hqSize(HQUEUE * phq);

#ifdef __cplusplus
}
#endif

#endif // HQUEUE_H


