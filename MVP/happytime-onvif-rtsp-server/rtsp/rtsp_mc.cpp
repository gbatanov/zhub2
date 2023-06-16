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
#include "rtsp_rsua.h"
#include "rtsp_mc.h"
#include "rtsp_srv.h"

/***********************************************************************/

extern RTSP_CLASS	hrtsp;

/***********************************************************************/
void rmc_proxy_init()
{
	hrtsp.rmc_fl = pps_ctx_fl_init(MAX_NUM_RUA, sizeof(RMCUA), TRUE);
	hrtsp.rmc_ul = pps_ctx_ul_init(hrtsp.rmc_fl, TRUE);
}

void rmc_proxy_deinit()
{
	if (hrtsp.rmc_ul)
	{
	    pps_ul_free(hrtsp.rmc_ul);
	    hrtsp.rmc_ul = NULL;
	}

	if (hrtsp.rmc_fl)
	{
	    pps_fl_free(hrtsp.rmc_fl);
	    hrtsp.rmc_fl = NULL;
	}
}

RMCUA * rmc_get_idle()
{
	RMCUA * p_rua = (RMCUA *)pps_fl_pop(hrtsp.rmc_fl);
	if (p_rua)
	{
		memset(p_rua, 0, sizeof(RMCUA));
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, don't have idle rtsp rua!!!\r\n", __FUNCTION__);
	}

	return p_rua;
}

void rmc_set_online(RMCUA * p_rua)
{
	pps_ctx_ul_add(hrtsp.rmc_ul, p_rua);
}

void rmc_set_idle(RMCUA * p_rua)
{
	pps_ctx_ul_del(hrtsp.rmc_ul, p_rua);

	sys_os_destroy_sig_mutex(p_rua->mutex);
	
	memset(p_rua, 0, sizeof(RMCUA));
	
	pps_fl_push(hrtsp.rmc_fl, p_rua);
}

RMCUA * rmc_lookup_start()
{
	return (RMCUA *)pps_lookup_start(hrtsp.rmc_ul);
}

RMCUA * rmc_lookup_next(RMCUA * p_rua)
{
	return (RMCUA *)pps_lookup_next(hrtsp.rmc_ul, p_rua);
}

void rmc_lookup_stop()
{
	pps_lookup_end(hrtsp.rmc_ul);
}

uint32 rmc_get_index(RMCUA * p_rua)
{
	return pps_get_index(hrtsp.rmc_fl, p_rua);
}

RMCUA * rmc_get_by_index(uint32 index)
{
	return (RMCUA *)pps_get_node_by_index(hrtsp.rmc_fl, index);
}

/***********************************************************************/

RMCUA * rtsp_mc_find(RSUA * p_rua)
{
    RMCUA * p_mcua = rmc_lookup_start();
    while (p_mcua)
    {
        if (strcmp(p_rua->uri, p_mcua->uri) == 0)
        {
            break;
        }
        
        p_mcua = rmc_lookup_next(p_mcua);
    }
    rmc_lookup_stop();

    return p_mcua;
}

BOOL rtsp_mc_add(RSUA * p_rua)
{
    RMCUA * p_mcua = rmc_get_idle();

    strcpy(p_mcua->uri, p_rua->uri);
    p_mcua->mcuaidx = rua_get_index(p_rua);
    p_mcua->refcnt = 1;
    p_mcua->mutex = sys_os_create_mutex();

    rmc_set_online(p_mcua);

    return TRUE;
}

int rtsp_mc_del(RSUA * p_rua)
{
    int refcnt = 0;
    RMCUA * p_mcua = rtsp_mc_find(p_rua);
    if (NULL == p_mcua)
    {
        return 0;
    }
    
    sys_os_mutex_enter(p_mcua->mutex);

    if (0 == p_rua->mc_del)
    {
        p_rua->mc_del = 1;
        p_mcua->refcnt--;
    }
    
    refcnt = p_mcua->refcnt;
    
    sys_os_mutex_leave(p_mcua->mutex);

    if (0 == refcnt)
    {
        rmc_set_idle(p_mcua);
    }

    return refcnt;
}

RMCUA * rtsp_mc_add_ref(RSUA * p_rua)
{
    RMCUA * p_mcua = rtsp_mc_find(p_rua);
    if (p_mcua)
    {
        sys_os_mutex_enter(p_mcua->mutex);

        p_mcua->refcnt++;

        sys_os_mutex_leave(p_mcua->mutex);
    }

    return p_mcua;
}

void rtsp_mc_del_ref(RSUA * p_rua)
{
    RMCUA * p_mcua = rtsp_mc_find(p_rua);
    if (NULL == p_mcua)
    {
        return;
    }
    
    sys_os_mutex_enter(p_mcua->mutex);

    if (0 == p_rua->mc_del)
    {
        p_rua->mc_del = 1;
        p_mcua->refcnt--;
    }

    sys_os_mutex_leave(p_mcua->mutex);
    
    if (0 == p_mcua->refcnt)
    {
        RSUA * p_mc = rua_get_by_index(p_mcua->mcuaidx);
        if (p_mc && p_mc->mc_del)
        {
            // close the multicast source
            rtsp_close_rua(p_mc);
        }

        rmc_set_idle(p_mcua);
    }
}



