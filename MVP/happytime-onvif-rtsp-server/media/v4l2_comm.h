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

#ifndef V4L2_COMMON_H
#define V4L2_COMMON_H

extern "C" 
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct fmt_map 
{
    enum AVPixelFormat ff_fmt;
    enum AVCodecID codec_id;
    uint32 v4l2_fmt;
};

#ifdef __cplusplus
extern "C" {
#endif

extern const struct fmt_map ff_fmt_conversion_table[];

uint32 ff_fmt_ff2v4l(enum AVPixelFormat pix_fmt, enum AVCodecID codec_id);
enum AVPixelFormat ff_fmt_v4l2ff(uint32 v4l2_fmt, enum AVCodecID codec_id);
enum AVCodecID ff_fmt_v4l2codec(uint32 v4l2_fmt);

#ifdef __cplusplus
}
#endif

#endif /* V4L2_COMMON_H */

