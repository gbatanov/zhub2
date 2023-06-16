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

#include "sys_inc.h"
#include "v4l2_comm.h"
#include <linux/videodev2.h>


const struct fmt_map ff_fmt_conversion_table[] = 
{
	{ AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV420  },
	{ AV_PIX_FMT_YUV420P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YVU420  },
	{ AV_PIX_FMT_YUV422P, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUV422P },
	{ AV_PIX_FMT_YUYV422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_YUYV    },
	{ AV_PIX_FMT_UYVY422, AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_UYVY    },
	{ AV_PIX_FMT_BGR24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR24   },
	{ AV_PIX_FMT_RGB24,   AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB24   },
	{ AV_PIX_FMT_BGR0,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_BGR32   },
    { AV_PIX_FMT_0RGB,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_RGB32   },
	{ AV_PIX_FMT_NV12,    AV_CODEC_ID_RAWVIDEO, V4L2_PIX_FMT_NV12    },
	{ AV_PIX_FMT_NONE,    AV_CODEC_ID_NONE,     0                    },
};


uint32 ff_fmt_ff2v4l(enum AVPixelFormat pix_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++) 
    {
        if ((codec_id == AV_CODEC_ID_NONE ||
             ff_fmt_conversion_table[i].codec_id == codec_id) &&
            (pix_fmt == AV_PIX_FMT_NONE ||
             ff_fmt_conversion_table[i].ff_fmt == pix_fmt)) 
        {
            return ff_fmt_conversion_table[i].v4l2_fmt;
        }
    }

    return 0;
}

enum AVPixelFormat ff_fmt_v4l2ff(uint32 v4l2_fmt, enum AVCodecID codec_id)
{
    int i;

    for (i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++) 
    {
        if (ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt &&
            ff_fmt_conversion_table[i].codec_id == codec_id) 
        {
            return ff_fmt_conversion_table[i].ff_fmt;
        }
    }

    return AV_PIX_FMT_NONE;
}

enum AVCodecID ff_fmt_v4l2codec(uint32 v4l2_fmt)
{
    int i;

    for (i = 0; ff_fmt_conversion_table[i].codec_id != AV_CODEC_ID_NONE; i++) 
    {
        if (ff_fmt_conversion_table[i].v4l2_fmt == v4l2_fmt) 
        {
            return ff_fmt_conversion_table[i].codec_id;
        }
    }

    return AV_CODEC_ID_NONE;
}


