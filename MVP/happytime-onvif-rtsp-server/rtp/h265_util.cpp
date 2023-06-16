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
#include "bs.h"
#include "h265.h"
#include "h265_util.h"


void h265_parser_init(h265_t * h)
{
	memset(h, 0, sizeof(h265_t));
}

int h265_parser_parse(h265_t * h, uint8 * p_data, int len)
{
	uint32 i;
	uint32 u_tmp;
	bs_t s;

	// ����Ԥ����
	uint8 bufs[512];
	if (len > sizeof(bufs))
	{
		return -1;
    }
    
	memcpy(bufs, p_data, len);

	uint8 * ptr = bufs;
	
	do {
		if (*ptr == 0x00 && *(ptr+1) == 0x00 && *(ptr+2) == 0x03)
		{
			memmove(ptr+2, ptr+3, len-(ptr-bufs)-2-1);
			ptr += 2;
			len--;
		}
		else
		{
			ptr++;
		}	
	} while ((ptr-bufs) < (len-3));

	bs_init(&s, bufs, len);

	u_tmp = bs_read(&s, 4);	// sps_video_parameter_set_id u(4)
	h->sps_max_sub_layers_minus1 = bs_read(&s, 3);	// sps_max_sub_layers_minus1 u(3)
	if (h->sps_max_sub_layers_minus1 > 6)
	{
		log_print(HT_LOG_ERR, "%s, sps_max_sub_layers_minus1[%d]>6!!!\r\n", __FUNCTION__, h->sps_max_sub_layers_minus1);
		return -1;
	}
	
	u_tmp = bs_read(&s, 1);	// sps_temporal_id_nesting_flag u(1)

	// profile_tier_level( maxNumSubLayersMinus1 )
	{
		u_tmp = bs_read(&s, 2);	    // general_profile_space u(2)
		u_tmp = bs_read(&s, 1);	    // general_tier_flag u(1)
		h->general_profile_idc = bs_read(&s, 5);	// general_profile_idc u(5)
		u_tmp = bs_read(&s, 32);	// general_profile_compatibility_flag[ j ] u(5)
		u_tmp = bs_read(&s, 1);	    // general_progressive_source_flag u(1)
		u_tmp = bs_read(&s, 1);	    // general_interlaced_source_flag u(1)
		u_tmp = bs_read(&s, 1);	    // general_non_packed_constraint_flag u(1)
		u_tmp = bs_read(&s, 1);	    // general_frame_only_constraint_flag u(1)
		bs_skip(&s, 44);			// general_reserved_zero_44bits u(44)
		h->general_level_idc = bs_read(&s, 8);	// general_level_idc u(8)

		uint8 sub_layer_profile_present_flag[6] = {0};
		uint8 sub_layer_level_present_flag[6]   = {0};
		
		for (i = 0; i < h->sps_max_sub_layers_minus1; i++) 
		{
			sub_layer_profile_present_flag[i]= bs_read(&s, 1);
			sub_layer_level_present_flag[i]= bs_read(&s, 1);
		}
		
		if (h->sps_max_sub_layers_minus1 > 0) 
		{
			for (i = h->sps_max_sub_layers_minus1; i < 8; i++) 
			{
				uint8 reserved_zero_2bits = bs_read(&s, 2);
			}
		}

		for (i = 0; i < h->sps_max_sub_layers_minus1; i++) 
		{
			if (sub_layer_profile_present_flag[i]) 
			{
				bs_read(&s, 2);	    // sub_layer_profile_space[i]
				bs_read(&s, 1);	    // sub_layer_tier_flag[i]
				bs_read(&s, 5);	    // sub_layer_profile_idc[i]
				bs_read(&s, 32);	// sub_layer_profile_compatibility_flag[i][32]
				bs_read(&s, 1);	    // sub_layer_progressive_source_flag[i]
				bs_read(&s, 1);	    // sub_layer_interlaced_source_flag[i]
				bs_read(&s, 1);	    // sub_layer_non_packed_constraint_flag[i]
				bs_read(&s, 1);	    // sub_layer_frame_only_constraint_flag[i]
				bs_read(&s, 44);	// sub_layer_reserved_zero_44bits[i]
			}
			
			if (sub_layer_level_present_flag[i])
			{
				bs_read(&s, 8);	// sub_layer_level_idc[i]
			}
		}
	}

	h->sps_seq_parameter_set_id = bs_read_ue(&s);	// sps_seq_parameter_set_id ue(v)
	h->chroma_format_idc = bs_read_ue(&s);	        // chroma_format_idc ue(v)
	if (h->sps_seq_parameter_set_id > 15 || h->chroma_format_idc > 3)
	{
		log_print(HT_LOG_ERR, "%s, sps_seq_parameter_set_id[%d],chroma_format_idc[%d]!!!\r\n", __FUNCTION__, h->sps_seq_parameter_set_id, h->chroma_format_idc);
		return -1;
	}

	if (h->chroma_format_idc == 3)
	{
		h->separate_colour_plane_flag = bs_read(&s, 1); // separate_colour_plane_flag
	}

	h->pic_width_in_luma_samples = bs_read_ue(&s);	    // pic_width_in_luma_samples ue(v)
	h->pic_height_in_luma_samples = bs_read_ue(&s);	    // pic_height_in_luma_samples ue(v)

	h->conformance_window_flag = bs_read(&s, 1 );	    // conformance_window_flag u(1)
	if (h->conformance_window_flag)
	{
		h->conf_win_left_offset = bs_read_ue(&s);	    // conf_win_left_offset ue(v)
		h->conf_win_right_offset = bs_read_ue(&s);	    // conf_win_right_offset ue(v)
		h->conf_win_top_offset = bs_read_ue(&s);		// conf_win_top_offset ue(v)
		h->conf_win_bottom_offset = bs_read_ue(&s);	    // conf_win_bottom_offset ue(v)
	}

	h->bit_depth_luma_minus8 = bs_read_ue(&s);		    // bit_depth_luma_minus8 ue(v)
	h->bit_depth_chroma_minus8 = bs_read_ue(&s);		// bit_depth_chroma_minus8 ue(v)

	return 0;
}




