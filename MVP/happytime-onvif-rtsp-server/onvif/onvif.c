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
#include "onvif.h"
#include "onvif_device.h"
#include "xml_node.h"
#include "onvif_event.h"
#include "onvif_utils.h"
#include "onvif_cfg.h"
#include "util.h"
#include <math.h>

#ifdef LIBICAL
#include "icalvcal.h"
#include "vcc.h"   
#endif

/***************************************************************************************/
ONVIF_CFG g_onvif_cfg;
ONVIF_CLS g_onvif_cls;


/***************************************************************************************/

HT_API BOOL onvif_is_scope_exist(const char * scope)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (strcmp(scope, g_onvif_cfg.scopes[i].ScopeItem) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;	
}

HT_API ONVIF_RET onvif_add_scope(const char * scope, BOOL fixed)
{
    onvif_Scope * p_scope;
    
	if (onvif_is_scope_exist(scope) == TRUE)
	{
		return ONVIF_ERR_ScopeOverwrite;
	}

	p_scope = onvif_get_idle_scope();
	if (p_scope)
	{
		p_scope->ScopeDef = fixed ? ScopeDefinition_Fixed : ScopeDefinition_Configurable;
		strncpy(p_scope->ScopeItem, scope, sizeof(p_scope->ScopeItem)-1);
		return ONVIF_OK;
	}

	return ONVIF_ERR_TooManyScopes;
}

HT_API onvif_Scope * onvif_find_scope(const char * scope)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (strcmp(g_onvif_cfg.scopes[i].ScopeItem, scope) == 0)
		{
			return &g_onvif_cfg.scopes[i];
		}
	}

	return NULL;
}

HT_API onvif_Scope * onvif_get_idle_scope()
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] == '\0')
		{
			return &g_onvif_cfg.scopes[i];
		}
	}

	return NULL;
}

HT_API BOOL onvif_is_user_exist(const char * username)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] != '\0' && 
		    strcmp(username, g_onvif_cfg.users[i].Username) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;	
}

HT_API ONVIF_RET onvif_add_user(onvif_User * p_user)
{
    onvif_User * p_idle_user;
    
	if (onvif_is_user_exist(p_user->Username) == TRUE)
	{
		return ONVIF_ERR_UsernameClash;
	}

	p_idle_user = onvif_get_idle_user();
	if (p_idle_user)
	{
		memcpy(p_idle_user, p_user, sizeof(onvif_User));
		return ONVIF_OK;
	}

	return ONVIF_ERR_TooManyUsers;
}

HT_API onvif_User * onvif_find_user(const char * username)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] != '\0' && 
		    strcmp(g_onvif_cfg.users[i].Username, username) == 0)
		{
			return &g_onvif_cfg.users[i];
		}
	}

	return NULL;
}

HT_API onvif_User * onvif_get_idle_user()
{
    int i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] == '\0')
		{
			return &g_onvif_cfg.users[i];
		}
	}

	return NULL;
}

HT_API const char * onvif_get_user_pass(const char * username)
{
    onvif_User * p_user;
    
    if (NULL == username || strlen(username) == 0)
    {
        return NULL;
    }
    
    p_user = onvif_find_user(username);
	if (NULL != p_user)
	{
	    return p_user->Password;
	}

	return NULL;	
}

HT_API ONVIF_PROFILE * onvif_add_profile(BOOL fixed)
{
    ONVIF_PROFILE * p_tmp;
    ONVIF_PROFILE * p_new = (ONVIF_PROFILE *) malloc(sizeof(ONVIF_PROFILE));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ONVIF_PROFILE));

	p_new->fixed = fixed;

	snprintf(p_new->name, ONVIF_NAME_LEN, "PROFILE_00%d", g_onvif_cls.profile_idx);
    snprintf(p_new->token, ONVIF_TOKEN_LEN, "PROFILE_00%d", g_onvif_cls.profile_idx);

    g_onvif_cls.profile_idx++;

	p_tmp = g_onvif_cfg.profiles;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.profiles = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API ONVIF_PROFILE * onvif_find_profile(ONVIF_PROFILE * p_head, const char * token)
{
    ONVIF_PROFILE * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_profiles(ONVIF_PROFILE ** p_head)
{
    ONVIF_PROFILE * p_next;
	ONVIF_PROFILE * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

#ifdef PTZ_SUPPORT
        onvif_free_PTZPresets(&p_tmp->presets);
        onvif_free_PresetTours(&p_tmp->preset_tour);
#endif

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoSourceList * onvif_add_VideoSource(int w, int h)
{
	VideoSourceList * p_tmp;
	VideoSourceList * p_new = (VideoSourceList *) malloc(sizeof(VideoSourceList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoSourceList));

	p_new->VideoSource.Framerate = 25;
	p_new->VideoSource.Resolution.Width = w;
	p_new->VideoSource.Resolution.Height = h;

	snprintf(p_new->VideoSource.token, ONVIF_TOKEN_LEN, "V_SRC_00%d", g_onvif_cls.v_src_idx);

    // init video source mode
    p_new->VideoSourceMode.Enabled = 1;
    p_new->VideoSourceMode.Reboot = 1;
    p_new->VideoSourceMode.MaxFramerate = 30.0f;
    p_new->VideoSourceMode.MaxResolution.Width = w;
    p_new->VideoSourceMode.MaxResolution.Height = h;
    snprintf(p_new->VideoSourceMode.token, ONVIF_TOKEN_LEN, "V_SRC_MODE_00%d", g_onvif_cls.v_src_idx);
    strcpy(p_new->VideoSourceMode.Encodings, "H264 MP4 MJPEG");

#ifdef THERMAL_SUPPORT
    p_new->ThermalSupport = onvif_init_Thermal(p_new);    
#endif

    p_tmp = g_onvif_cfg.v_src;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.v_src = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API VideoSourceList * onvif_find_VideoSource(VideoSourceList * p_head, const char * token)
{
	VideoSourceList * p_tmp = p_head;
    while (p_tmp)
	{
	    if (strcmp(p_tmp->VideoSource.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API VideoSourceList * onvif_find_VideoSource_by_size(VideoSourceList * p_head, int w, int h)
{
	VideoSourceList * p_tmp = p_head;
    while (p_tmp)
    {
        if (p_tmp->VideoSource.Resolution.Width == w && p_tmp->VideoSource.Resolution.Height == h)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_VideoSource(VideoSourceList * p_node)
{
#ifdef THERMAL_SUPPORT
    if (p_node->ThermalConfigurationOptions.ColorPalette)
    {
        onvif_free_ColorPalettes(&p_node->ThermalConfigurationOptions.ColorPalette);
    }

    if (p_node->ThermalConfigurationOptions.NUCTable)
    {
        onvif_free_NUCTables(&p_node->ThermalConfigurationOptions.NUCTable);
    }
#endif    
}

HT_API void onvif_free_VideoSources(VideoSourceList ** p_head)
{
    VideoSourceList * p_next;
	VideoSourceList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_VideoSource(p_tmp);
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoSourceConfigurationList * onvif_add_VideoSourceConfiguration(int w, int h)
{
    VideoSourceList * p_v_src;
    VideoSourceConfigurationList * p_tmp;
	VideoSourceConfigurationList * p_new = (VideoSourceConfigurationList *) malloc(sizeof(VideoSourceConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoSourceConfigurationList));

	p_new->Configuration.Bounds.x = 0;
	p_new->Configuration.Bounds.y = 0;
	p_new->Configuration.Bounds.width = w;
	p_new->Configuration.Bounds.height = h;

	p_new->Configuration.UseCount = 0;

	snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "V_SRC_CFG_00%d", g_onvif_cls.v_src_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "V_SRC_CFG_00%d", g_onvif_cls.v_src_idx);

    p_v_src = onvif_find_VideoSource_by_size(g_onvif_cfg.v_src, w, h);
	if (NULL == p_v_src)
	{
		p_v_src = onvif_add_VideoSource(w, h);
	}
	
	strcpy(p_new->Configuration.SourceToken, p_v_src->VideoSource.token);

    g_onvif_cls.v_src_idx++;

	p_tmp = g_onvif_cfg.v_src_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.v_src_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API VideoSourceConfigurationList * onvif_find_VideoSourceConfiguration(VideoSourceConfigurationList * p_head, const char * token)
{
    VideoSourceConfigurationList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API VideoSourceConfigurationList * onvif_find_VideoSourceConfiguration_by_size(VideoSourceConfigurationList * p_head, int w, int h)
{
	VideoSourceConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.Bounds.width == w && p_tmp->Configuration.Bounds.height == h)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_VideoSourceConfigurations(VideoSourceConfigurationList ** p_head)
{
    VideoSourceConfigurationList * p_next;
	VideoSourceConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoEncoder2ConfigurationList * onvif_add_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_node)
{
    VideoEncoder2ConfigurationList * p_tmp;
	VideoEncoder2ConfigurationList * p_new = (VideoEncoder2ConfigurationList *) malloc(sizeof(VideoEncoder2ConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoEncoder2ConfigurationList));
	
	memcpy(&p_new->Configuration, &p_node->Configuration, sizeof(onvif_VideoEncoder2Configuration));

	snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "V_ENC_00%d", g_onvif_cls.v_enc_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "V_ENC_00%d", g_onvif_cls.v_enc_idx);

	onvif_init_MulticastConfiguration(&p_new->Configuration.Multicast);	
	
    g_onvif_cls.v_enc_idx++;

	p_tmp = g_onvif_cfg.v_enc_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.v_enc_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
    
	return p_new;
}

HT_API VideoEncoder2ConfigurationList * onvif_find_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_head, const char * token)
{
	VideoEncoder2ConfigurationList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API VideoEncoder2ConfigurationList * onvif_find_VideoEncoder2Configuration_by_param(VideoEncoder2ConfigurationList * p_head, VideoEncoder2ConfigurationList * p_node)
{
	VideoEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.Resolution.Width == p_node->Configuration.Resolution.Width && 
			p_tmp->Configuration.Resolution.Height == p_node->Configuration.Resolution.Height && 
			fabs(p_tmp->Configuration.Quality - p_node->Configuration.Quality) < 0.1 && 
			p_tmp->Configuration.SessionTimeout == p_node->Configuration.SessionTimeout && 
			fabs(p_tmp->Configuration.RateControl.FrameRateLimit - p_node->Configuration.RateControl.FrameRateLimit) < 0.1 && 
			p_tmp->Configuration.RateControl.EncodingInterval == p_node->Configuration.RateControl.EncodingInterval && 
			p_tmp->Configuration.RateControl.BitrateLimit == p_node->Configuration.RateControl.BitrateLimit && 
			strcmp(p_tmp->Configuration.Encoding, p_node->Configuration.Encoding) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_VideoEncoder2Configurations(VideoEncoder2ConfigurationList ** p_head)
{
    VideoEncoder2ConfigurationList * p_next;
	VideoEncoder2ConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API NetworkInterfaceList * onvif_add_NetworkInterface()
{
    NetworkInterfaceList * p_tmp;
	NetworkInterfaceList * p_new = (NetworkInterfaceList *) malloc(sizeof(NetworkInterfaceList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(NetworkInterfaceList));

    snprintf(p_new->NetworkInterface.token, ONVIF_TOKEN_LEN, "eth%d", g_onvif_cls.netinf_idx);

    g_onvif_cls.netinf_idx++;

	p_tmp = g_onvif_cfg.network.interfaces;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.network.interfaces = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API NetworkInterfaceList * onvif_find_NetworkInterface(NetworkInterfaceList * p_head, const char * token)
{
    NetworkInterfaceList * p_tmp = p_head;

    while (p_tmp)
    {
        if (strcmp(p_tmp->NetworkInterface.token, token) == 0)
        {
            break;
        }

        p_tmp = p_tmp->next;
    }

    return p_tmp;
}

HT_API void onvif_free_NetworkInterfaces(NetworkInterfaceList ** p_head)
{
    NetworkInterfaceList * p_next;
	NetworkInterfaceList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API OSDConfigurationList * onvif_add_OSDConfiguration()
{
    OSDConfigurationList * p_tmp;
	OSDConfigurationList * p_new = (OSDConfigurationList *) malloc(sizeof(OSDConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(OSDConfigurationList));

	p_tmp = g_onvif_cfg.OSDs;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.OSDs = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API OSDConfigurationList * onvif_find_OSDConfiguration(OSDConfigurationList * p_head, const char * token)
{
	OSDConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->OSD.token, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_OSDConfigurations(OSDConfigurationList ** p_head)
{
    OSDConfigurationList * p_next;
	OSDConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API MetadataConfigurationList * onvif_add_MetadataConfiguration(MetadataConfigurationList ** p_head)
{
    MetadataConfigurationList * p_tmp;
	MetadataConfigurationList * p_new = (MetadataConfigurationList *) malloc(sizeof(MetadataConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(MetadataConfigurationList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API MetadataConfigurationList * onvif_find_MetadataConfiguration(MetadataConfigurationList * p_head, const char * token)
{
	MetadataConfigurationList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_MetadataConfigurations(MetadataConfigurationList ** p_head)
{
    MetadataConfigurationList * p_next;
	MetadataConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoEncoder2ConfigurationOptionsList * onvif_add_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head)
{
    VideoEncoder2ConfigurationOptionsList * p_tmp;
	VideoEncoder2ConfigurationOptionsList * p_new = (VideoEncoder2ConfigurationOptionsList *) malloc(sizeof(VideoEncoder2ConfigurationOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoEncoder2ConfigurationOptionsList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API VideoEncoder2ConfigurationOptionsList * onvif_find_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList * p_head, const char * encoding)
{
    VideoEncoder2ConfigurationOptionsList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Options.Encoding, encoding) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head)
{
    VideoEncoder2ConfigurationOptionsList * p_next;
	VideoEncoder2ConfigurationOptionsList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API NotificationMessageList * onvif_add_NotificationMessage(NotificationMessageList ** p_head)
{
	NotificationMessageList * p_tmp;
	NotificationMessageList * p_new = (NotificationMessageList *) malloc(sizeof(NotificationMessageList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(NotificationMessageList));

	if (p_head)
	{
		p_tmp = *p_head;
		if (NULL == p_tmp)
		{
			*p_head = p_new;
		}
		else
		{
			while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

			p_tmp->next = p_new;
		}	
	}

	p_new->refcnt++;

	return p_new;
}

HT_API void onvif_free_NotificationMessage(NotificationMessageList * p_message)
{
	if (p_message)
	{
	    p_message->refcnt--;
	    if (p_message->refcnt <= 0)
	    {
	        onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Source.SimpleItem);
    		onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Key.SimpleItem);
    		onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Data.SimpleItem);

    		onvif_free_ElementItems(&p_message->NotificationMessage.Message.Source.ElementItem);
    		onvif_free_ElementItems(&p_message->NotificationMessage.Message.Key.ElementItem);
    		onvif_free_ElementItems(&p_message->NotificationMessage.Message.Data.ElementItem);

    		free(p_message);
	    }
	}
}

HT_API void onvif_free_NotificationMessages(NotificationMessageList ** p_head)
{
	NotificationMessageList * p_next;
	NotificationMessageList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_NotificationMessage(p_tmp);
		
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API SimpleItemList * onvif_add_SimpleItem(SimpleItemList ** p_head)
{
	SimpleItemList * p_tmp;
	SimpleItemList * p_new = (SimpleItemList *) malloc(sizeof(SimpleItemList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(SimpleItemList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_SimpleItems(SimpleItemList ** p_head)
{
    SimpleItemList * p_next;
	SimpleItemList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
		
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API ElementItemList * onvif_add_ElementItem(ElementItemList ** p_head)
{
	ElementItemList * p_tmp;
	ElementItemList * p_new = (ElementItemList *) malloc(sizeof(ElementItemList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ElementItemList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_ElementItems(ElementItemList ** p_head)
{
    ElementItemList * p_next;
	ElementItemList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		if (p_tmp->ElementItem.Any)
		{
		    free(p_tmp->ElementItem.Any);
		}
		
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

#ifdef IMAGE_SUPPORT

HT_API ImagingPresetList * onvif_add_ImagingPreset(ImagingPresetList ** p_head)
{
    ImagingPresetList * p_tmp;
    ImagingPresetList * p_new = (ImagingPresetList *) malloc(sizeof(ImagingPresetList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ImagingPresetList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API ImagingPresetList * onvif_find_ImagingPreset(ImagingPresetList * p_head, const char * token)
{
    ImagingPresetList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Preset.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_ImagingPresets(ImagingPresetList ** p_head)
{
    ImagingPresetList * p_next;
	ImagingPresetList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

#endif // IMAGE_SUPPORT

#ifdef AUDIO_SUPPORT

HT_API AudioSourceList * onvif_add_AudioSource(AudioSourceList ** p_head)
{
    AudioSourceList * p_tmp;
    AudioSourceList * p_new = (AudioSourceList *) malloc(sizeof(AudioSourceList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioSourceList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API AudioSourceList * onvif_find_AudioSource(AudioSourceList * p_head, const char * token)
{
    AudioSourceList * p_tmp = p_head;
    
    while (p_tmp)
	{
		if (strcmp(p_tmp->AudioSource.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_AudioSources(AudioSourceList ** p_head)
{
    AudioSourceList * p_next;
	AudioSourceList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioSourceConfigurationList * onvif_add_AudioSourceConfiguration()
{
    AudioSourceConfigurationList * p_tmp;
	AudioSourceConfigurationList * p_new = (AudioSourceConfigurationList *) malloc(sizeof(AudioSourceConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioSourceConfigurationList));

	snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "A_SRC_CFG_00%d", g_onvif_cls.a_src_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "A_SRC_CFG_00%d", g_onvif_cls.a_src_idx);

    assert(g_onvif_cfg.a_src);
    strcpy(p_new->Configuration.SourceToken, g_onvif_cfg.a_src->AudioSource.token);

    g_onvif_cls.a_src_idx++;

	p_tmp = g_onvif_cfg.a_src_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.a_src_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API AudioSourceConfigurationList * onvif_find_AudioSourceConfiguration(AudioSourceConfigurationList * p_head, const char * token)
{
    AudioSourceConfigurationList * p_tmp = p_head;

    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_AudioSourceConfigurations(AudioSourceConfigurationList ** p_head)
{
    AudioSourceConfigurationList * p_next;
	AudioSourceConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioEncoder2ConfigurationList * onvif_add_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_node)
{
	AudioEncoder2ConfigurationList * p_tmp;
	AudioEncoder2ConfigurationList * p_new = (AudioEncoder2ConfigurationList *) malloc(sizeof(AudioEncoder2ConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	p_new->next = NULL;
	memcpy(&p_new->Configuration, &p_node->Configuration, sizeof(onvif_AudioEncoder2Configuration));

	snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "A_ENC_00%d", g_onvif_cls.a_enc_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "A_ENC_00%d", g_onvif_cls.a_enc_idx);

	onvif_init_MulticastConfiguration(&p_new->Configuration.Multicast);
	
    g_onvif_cls.a_enc_idx++;

	p_tmp = g_onvif_cfg.a_enc_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.a_enc_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API AudioEncoder2ConfigurationList * onvif_find_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_head, const char * token)
{
    AudioEncoder2ConfigurationList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API AudioEncoder2ConfigurationList * onvif_find_AudioEncoder2Configuration_by_param(AudioEncoder2ConfigurationList * p_head, AudioEncoder2ConfigurationList * p_node)
{
	AudioEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.SessionTimeout == p_node->Configuration.SessionTimeout &&
			p_tmp->Configuration.SampleRate == p_node->Configuration.SampleRate && 
			p_tmp->Configuration.Bitrate == p_node->Configuration.Bitrate && 
			strcmp(p_tmp->Configuration.Encoding, p_node->Configuration.Encoding) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_AudioEncoder2Configurations(AudioEncoder2ConfigurationList ** p_head)
{
    AudioEncoder2ConfigurationList * p_next;
	AudioEncoder2ConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioEncoder2ConfigurationOptionsList * onvif_add_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head)
{
    AudioEncoder2ConfigurationOptionsList * p_tmp;
	AudioEncoder2ConfigurationOptionsList * p_new = (AudioEncoder2ConfigurationOptionsList *) malloc(sizeof(AudioEncoder2ConfigurationOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioEncoder2ConfigurationOptionsList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API AudioEncoder2ConfigurationOptionsList * onvif_find_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList * p_head, const char * encoding)
{
    AudioEncoder2ConfigurationOptionsList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Options.Encoding, encoding) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head)
{
    AudioEncoder2ConfigurationOptionsList * p_next;
	AudioEncoder2ConfigurationOptionsList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioDecoderConfigurationList * onvif_add_AudioDecoderConfiguration()
{
    AudioDecoderConfigurationList * p_tmp;
	AudioDecoderConfigurationList * p_new = (AudioDecoderConfigurationList *) malloc(sizeof(AudioDecoderConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioDecoderConfigurationList));

	snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "A_DEC_00%d", g_onvif_cls.a_dec_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "A_DEC_00%d", g_onvif_cls.a_dec_idx);
	
    g_onvif_cls.a_dec_idx++;

	p_tmp = g_onvif_cfg.a_dec_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.a_dec_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API AudioDecoderConfigurationList * onvif_find_AudioDecoderConfiguration(AudioDecoderConfigurationList * p_head, const char * token)
{
    AudioDecoderConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_AudioDecoderConfigurations(AudioDecoderConfigurationList ** p_head)
{
    AudioDecoderConfigurationList * p_next;
	AudioDecoderConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

#if defined(MEDIA2_SUPPORT)
        onvif_free_AudioEncoder2ConfigurationOptions(&p_tmp->Options2);
#endif

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

/*
 * Initialize the audio source
 * 
 */
void onvif_init_AudioSource()
{
	// todo : here init one audio source (2 channels)

	AudioSourceList * p_node = onvif_add_AudioSource(&g_onvif_cfg.a_src);
	if (NULL == p_node)
	{
		return;
	}

	memset(p_node, 0, sizeof(AudioSourceList));

	p_node->AudioSource.Channels = 2;
	strcpy(p_node->AudioSource.token, "AudioSourceToken");
}

void onvif_init_AudioEncoder2ConfigurationOptions(onvif_AudioEncoder2ConfigurationOptions * p_option, const char * Encoding)
{
    strcpy(p_option->Encoding, Encoding);
    if (strcasecmp(Encoding, "PCMU") == 0)
    {
        p_option->AudioEncoding = AudioEncoding_G711;
    }
    else if (strcasecmp(Encoding, "G726") == 0)
    {
        p_option->AudioEncoding = AudioEncoding_G726;
    }
    else if (strcasecmp(Encoding, "MP4A-LATM") == 0)
    {
        p_option->AudioEncoding = AudioEncoding_AAC;
    }

    p_option->BitrateList.sizeItems = 5;
	p_option->BitrateList.Items[0] = 64;
	p_option->BitrateList.Items[1] = 80;
	p_option->BitrateList.Items[2] = 128;
	p_option->BitrateList.Items[3] = 256;
	p_option->BitrateList.Items[4] = 320;
	
	// specify the supported samplerate
	p_option->SampleRateList.sizeItems = 5;
	p_option->SampleRateList.Items[0] = 8;
	p_option->SampleRateList.Items[1] = 12;
	p_option->SampleRateList.Items[2] = 25;
	p_option->SampleRateList.Items[3] = 32;
	p_option->SampleRateList.Items[4] = 48;
}

/*
 * Initialize the audio encoder configuration options
 * 
 */
void onvif_init_AudioEncoderConfigurationOptions()
{
    AudioEncoder2ConfigurationOptionsList * p_option;

    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&g_onvif_cfg.a_enc_cfg_opt);

    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "PCMU");

    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&g_onvif_cfg.a_enc_cfg_opt);

    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G726");

    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&g_onvif_cfg.a_enc_cfg_opt);

    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "MP4A-LATM");    
}

void onvif_init_AudioDecoderConfigurations()
{
    AudioDecoderConfigurationList * p_a_dec_cfg = onvif_add_AudioDecoderConfiguration();
#ifdef MEDIA2_SUPPORT
    AudioEncoder2ConfigurationOptionsList * p_option;
#endif

    p_a_dec_cfg->Options.G711DecOptionsFlag = 1;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.sizeItems = 6;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[0] = 8;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[1] = 12;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[2] = 20;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[3] = 25;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[4] = 32;
    p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[5] = 40;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.sizeItems = 5;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[0] = 8;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[1] = 12;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[2] = 24;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[3] = 32;
    p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "PCMU");
#endif

    p_a_dec_cfg = onvif_add_AudioDecoderConfiguration();

    p_a_dec_cfg->Options.G726DecOptionsFlag = 1;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.sizeItems = 6;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[0] = 8;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[1] = 12;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[2] = 20;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[3] = 25;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[4] = 32;
    p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[5] = 40;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.sizeItems = 5;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[0] = 8;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[1] = 12;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[2] = 24;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[3] = 32;
    p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G726");
#endif

    p_a_dec_cfg = onvif_add_AudioDecoderConfiguration();

    p_a_dec_cfg->Options.AACDecOptionsFlag = 1;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.sizeItems = 6;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[0] = 8;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[1] = 12;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[2] = 20;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[3] = 25;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[4] = 32;
    p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[5] = 40;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.sizeItems = 5;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[0] = 8;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[1] = 12;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[2] = 24;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[3] = 32;
    p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
    p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
    onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "MP4A-LATM");
#endif
}

#endif // end of AUDIO_SUPPORT

#ifdef MEDIA2_SUPPORT

HT_API MaskList * onvif_add_Mask(MaskList ** p_head)
{
    MaskList * p_tmp;
	MaskList * p_new = (MaskList *) malloc(sizeof(MaskList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(MaskList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API MaskList * onvif_find_Mask(MaskList * p_head, const char * token)
{
    MaskList * p_tmp = p_head;

    if (NULL == token)
    {
        return NULL;
    }
    
    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Mask.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_Masks(MaskList ** p_head)
{
    MaskList * p_next;
	MaskList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_MaskOptions()
{
    g_onvif_cfg.MaskOptions.MaxMasks = 10;
    g_onvif_cfg.MaskOptions.MaxPoints = 10;

    g_onvif_cfg.MaskOptions.sizeTypes = 3;
    strcpy(g_onvif_cfg.MaskOptions.Types[0], "Color");
    strcpy(g_onvif_cfg.MaskOptions.Types[1], "Pixelated");
    strcpy(g_onvif_cfg.MaskOptions.Types[2], "Blurred");

    g_onvif_cfg.MaskOptions.Color.sizeColorList = 1;
    g_onvif_cfg.MaskOptions.Color.ColorList[0].X = 100;
    g_onvif_cfg.MaskOptions.Color.ColorList[0].Y = 100;
    g_onvif_cfg.MaskOptions.Color.ColorList[0].Z = 100;
    g_onvif_cfg.MaskOptions.Color.ColorList[0].ColorspaceFlag = 1;
    strcpy(g_onvif_cfg.MaskOptions.Color.ColorList[0].Colorspace, "http://www.onvif.org/ver10/colorspace/YCbCr");
    g_onvif_cfg.MaskOptions.Color.sizeColorspaceRange = 0;

    g_onvif_cfg.MaskOptions.RectangleOnly = TRUE;
    g_onvif_cfg.MaskOptions.SingleColorOnly = FALSE;
}

#endif // end of MEDIA2_SUPPORT

#ifdef PTZ_SUPPORT

HT_API PTZNodeList * onvif_add_PTZNode(PTZNodeList ** p_head)
{
    PTZNodeList * p_tmp;
    PTZNodeList * p_new = (PTZNodeList *) malloc(sizeof(PTZNodeList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PTZNodeList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API PTZNodeList * onvif_find_PTZNode(PTZNodeList * p_head, const char * token)
{
    PTZNodeList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->PTZNode.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_PTZNodes(PTZNodeList ** p_head)
{
    PTZNodeList * p_next;
	PTZNodeList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API PTZConfigurationList * onvif_add_PTZConfiguration(PTZConfigurationList ** p_head)
{
    PTZConfigurationList * p_tmp;
    PTZConfigurationList * p_new = (PTZConfigurationList *) malloc(sizeof(PTZConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PTZConfigurationList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API PTZConfigurationList * onvif_find_PTZConfiguration(PTZConfigurationList * p_head, const char * token)
{
    PTZConfigurationList * p_tmp = p_head;
    
    while (p_tmp)
	{
	    if (strcmp(p_tmp->Configuration.token, token) == 0)
	    {
	        break;
	    }
	    
		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_PTZConfigurations(PTZConfigurationList ** p_head)
{
    PTZConfigurationList * p_next;
	PTZConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API PTZPresetList * onvif_add_PTZPreset(PTZPresetList ** p_head)
{
    PTZPresetList * p_tmp;
    PTZPresetList * p_new = (PTZPresetList *) malloc(sizeof(PTZPresetList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PTZPresetList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API PTZPresetList * onvif_find_PTZPreset(PTZPresetList * p_head, const char  * preset_token)
{
    PTZPresetList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->PTZPreset.token, preset_token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_PTZPreset(PTZPresetList ** p_head, PTZPresetList * p_node)
{
    PTZPresetList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}
    
	free(p_node);
}

HT_API void onvif_free_PTZPresets(PTZPresetList ** p_head)
{
    PTZPresetList * p_next;
	PTZPresetList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API PTZPresetTourSpotList * onvif_add_PTZPresetTourSpot(PTZPresetTourSpotList ** p_head)
{
	PTZPresetTourSpotList * p_tmp;
    PTZPresetTourSpotList * p_new = (PTZPresetTourSpotList *) malloc(sizeof(PTZPresetTourSpotList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PTZPresetTourSpotList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API void onvif_free_PTZPresetTourSpots(PTZPresetTourSpotList ** p_head)
{
	PTZPresetTourSpotList * p_next;
	PTZPresetTourSpotList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API PresetTourList * onvif_add_PresetTour(PresetTourList ** p_head)
{
	PresetTourList * p_tmp;
    PresetTourList * p_new = (PresetTourList *) malloc(sizeof(PresetTourList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PresetTourList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	
	return p_new;
}

HT_API PresetTourList * onvif_find_PresetTour(PresetTourList * p_head, const char * token)
{
    PresetTourList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->PresetTour.token, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_PresetTour(PresetTourList ** p_head, PresetTourList * p_node)
{
    PresetTourList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

    onvif_free_PTZPresetTourSpots(&p_node->PresetTour.TourSpot);
    
	free(p_node);
}

HT_API void onvif_free_PresetTours(PresetTourList ** p_head)
{
	PresetTourList * p_next;
	PresetTourList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_PTZPresetTourSpots(&p_tmp->PresetTour.TourSpot);
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API int onvif_count_PresetTours(PresetTourList * p_head)
{
    int count = 0;
    
	PresetTourList * p_tmp = p_head;

	while (p_tmp)
	{
		count++;
		
		p_tmp = p_tmp->next;
	}

	return count;
}

/**
 * init PTZ node
 */
void onvif_init_PTZNode()
{
    // todo : init one ptz node

    PTZNodeList * p_node = onvif_add_PTZNode(&g_onvif_cfg.ptz_node);
	if (NULL == p_node)
	{
		return;
	}

	memset(p_node, 0, sizeof(PTZNodeList));

	strcpy(p_node->PTZNode.Name, "PTZNODE_000");
    strcpy(p_node->PTZNode.token, "PTZNODE_000");

    p_node->PTZNode.NameFlag = 1;
    
    p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max = 1.0;
    p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max = 1.0;
    
    p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Min = 0.0;
    p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Max = 1.0;
    p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Max = 1.0;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Min = -1.0;
    p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Min = 0.0;
    p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Max = 1.0;

    p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpaceFlag = 1;
    p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpace.XRange.Min = 0.0;
    p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpace.XRange.Max = 1.0;

    p_node->PTZNode.MaximumNumberOfPresets = MAX_PTZ_PRESETS;
    p_node->PTZNode.HomeSupported = TRUE;
    
    p_node->PTZNode.ExtensionFlag = 1;
    p_node->PTZNode.Extension.SupportedPresetTourFlag = 1;
    p_node->PTZNode.Extension.SupportedPresetTour.MaximumNumberOfPresetTours = 10;
    p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Start = 1;
    p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Stop = 1;
    p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Pause = 1;
    p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Extended = 0;

    p_node->PTZNode.FixedHomePosition = FALSE;
    p_node->PTZNode.GeoMove = TRUE;

    p_node->PTZNode.sizeAuxiliaryCommands = 2;
    strcpy(p_node->PTZNode.AuxiliaryCommands[0], "Wiper start");
    strcpy(p_node->PTZNode.AuxiliaryCommands[1], "Wiper stop");
}

/**
 * init ptz configuration
 */
void onvif_init_PTZConfiguration()
{
    PTZConfigurationList * p_node = onvif_add_PTZConfiguration(&g_onvif_cfg.ptz_cfg);
	if (NULL == p_node)
	{
		return;
	}

	memset(p_node, 0, sizeof(PTZConfigurationList));

	strcpy(p_node->Configuration.Name, "PTZCFG_000");
    strcpy(p_node->Configuration.token, "PTZCFG_000");

    assert(g_onvif_cfg.ptz_node);
    strcpy(p_node->Configuration.NodeToken, g_onvif_cfg.ptz_node->PTZNode.token);
    
    p_node->Configuration.DefaultPTZSpeedFlag = 1;
    p_node->Configuration.DefaultPTZSpeed.PanTiltFlag = 1;
    p_node->Configuration.DefaultPTZSpeed.PanTilt.x = 0.5;
    p_node->Configuration.DefaultPTZSpeed.PanTilt.y = 0.5;
    p_node->Configuration.DefaultPTZSpeed.ZoomFlag = 1;
    p_node->Configuration.DefaultPTZSpeed.Zoom.x = 0.5;

    p_node->Configuration.DefaultPTZTimeoutFlag = 1;
    p_node->Configuration.DefaultPTZTimeout = 5;

    p_node->Configuration.PanTiltLimitsFlag = 1;
    p_node->Configuration.PanTiltLimits.XRange.Min = -1.0;
    p_node->Configuration.PanTiltLimits.XRange.Max = 1.0;
    p_node->Configuration.PanTiltLimits.YRange.Min = -1.0;
    p_node->Configuration.PanTiltLimits.YRange.Max = 1.0;

    p_node->Configuration.ZoomLimitsFlag = 1;
    p_node->Configuration.ZoomLimits.XRange.Min = 0;
    p_node->Configuration.ZoomLimits.XRange.Max = 1.0;

    p_node->Configuration.ExtensionFlag = 1;
    p_node->Configuration.Extension.PTControlDirectionFlag = 1;
    p_node->Configuration.Extension.PTControlDirection.EFlipFlag = 1;
    p_node->Configuration.Extension.PTControlDirection.EFlip = EFlipMode_OFF;
    p_node->Configuration.Extension.PTControlDirection.ReverseFlag = 1;
    p_node->Configuration.Extension.PTControlDirection.Reverse= ReverseMode_OFF;
}

void onvif_init_PTZConfigurationOptions()
{
    g_onvif_cfg.PTZConfigurationOptions.PTZTimeout.Min = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTZTimeout.Max = 100;

    g_onvif_cfg.PTZConfigurationOptions.PTControlDirectionFlag = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTControlDirection.EFlipMode_OFF = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTControlDirection.EFlipMode_ON = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTControlDirection.ReverseMode_OFF = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTControlDirection.ReverseMode_ON = 1;
    g_onvif_cfg.PTZConfigurationOptions.PTControlDirection.ReverseMode_AUTO = 1;
}

void onvif_init_ptz()
{
    onvif_init_PTZNode();

    onvif_init_PTZConfiguration();

    onvif_init_PTZConfigurationOptions();
}

#endif // end of PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

HT_API ConfigList * onvif_add_Config(ConfigList ** p_head)
{
	ConfigList * p_tmp;
	ConfigList * p_new = (ConfigList *) malloc(sizeof(ConfigList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ConfigList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_Config(ConfigList * p_node)
{
	onvif_free_SimpleItems(&p_node->Config.Parameters.SimpleItem);
	onvif_free_ElementItems(&p_node->Config.Parameters.ElementItem);
}

HT_API void onvif_free_Configs(ConfigList ** p_head)
{
	ConfigList * p_next;
	ConfigList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_Config(p_tmp);
		
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API ConfigList * onvif_find_Config(ConfigList * p_head, const char * name)
{
	ConfigList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Config.Name, name) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_remove_Config(ConfigList ** p_head, ConfigList * p_remove)
{
	BOOL found = FALSE;
	ConfigList * p_prev = NULL;
	ConfigList * p_cfg = *p_head;	
	
	while (p_cfg)
	{
		if (p_cfg == p_remove)
		{
			found = TRUE;
			break;
		}

		p_prev = p_cfg;
		p_cfg = p_cfg->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_cfg->next;
		}
		else
		{
			p_prev->next = p_cfg->next;
		}

		onvif_free_Config(p_cfg);
		free(p_cfg);
	}
}

HT_API ConfigList * onvif_get_prev_Config(ConfigList * p_head, ConfigList * p_found)
{
	ConfigList * p_prev = p_head;
	
	if (p_found == p_head)
	{
		return NULL;
	}

	while (p_prev)
	{
		if (p_prev->next == p_found)
		{
			break;
		}
		
		p_prev = p_prev->next;
	}

	return p_prev;
}

HT_API ConfigDescriptionList * onvif_add_ConfigDescription(ConfigDescriptionList ** p_head)
{
    ConfigDescriptionList * p_tmp;
	ConfigDescriptionList * p_new = (ConfigDescriptionList *) malloc(sizeof(ConfigDescriptionList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ConfigDescriptionList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_ConfigDescriptions(ConfigDescriptionList ** p_head)
{
    ConfigDescriptionList * p_next;
	ConfigDescriptionList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_SimpleItemDescriptions(&p_tmp->ConfigDescription.Parameters.SimpleItemDescription);
        onvif_free_SimpleItemDescriptions(&p_tmp->ConfigDescription.Parameters.ElementItemDescription);
        
        onvif_free_ConfigDescription_Messages(&p_tmp->ConfigDescription.Messages);

        onvif_free_ConfigOptions(&p_tmp->ConfigOptions);
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API ConfigDescription_MessagesList * onvif_add_ConfigDescription_Message(ConfigDescription_MessagesList ** p_head)
{
    ConfigDescription_MessagesList * p_tmp;
	ConfigDescription_MessagesList * p_new = (ConfigDescription_MessagesList *) malloc(sizeof(ConfigDescription_MessagesList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ConfigDescription_MessagesList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_ConfigDescription_Message(ConfigDescription_MessagesList * p_item)
{
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Source.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Source.ElementItemDescription);

	onvif_free_SimpleItemDescriptions(&p_item->Messages.Key.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Key.ElementItemDescription);

	onvif_free_SimpleItemDescriptions(&p_item->Messages.Data.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Data.ElementItemDescription);
}

HT_API void onvif_free_ConfigDescription_Messages(ConfigDescription_MessagesList ** p_head)
{
    ConfigDescription_MessagesList * p_next;
	ConfigDescription_MessagesList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
		
		onvif_free_ConfigDescription_Message(p_tmp);

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API ConfigOptionsList * onvif_add_ConfigOptions(ConfigOptionsList ** p_head)
{
    ConfigOptionsList * p_tmp;
	ConfigOptionsList * p_new = (ConfigOptionsList *) malloc(sizeof(ConfigOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ConfigOptionsList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_ConfigOptions(ConfigOptionsList ** p_head)
{
    ConfigOptionsList * p_next;
	ConfigOptionsList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        if (p_tmp->Options.any)
        {
            free(p_tmp->Options.any);
        }

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API SimpleItemDescriptionList * onvif_add_SimpleItemDescription(SimpleItemDescriptionList ** p_head)
{
    SimpleItemDescriptionList * p_tmp;
	SimpleItemDescriptionList * p_new = (SimpleItemDescriptionList *) malloc(sizeof(SimpleItemDescriptionList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(SimpleItemDescriptionList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_SimpleItemDescriptions(SimpleItemDescriptionList ** p_head)
{
    SimpleItemDescriptionList * p_next;
	SimpleItemDescriptionList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoAnalyticsConfigurationList * onvif_add_VideoAnalyticsConfiguration()
{
    VideoAnalyticsConfigurationList * p_tmp;
	VideoAnalyticsConfigurationList * p_new = (VideoAnalyticsConfigurationList *) malloc(sizeof(VideoAnalyticsConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoAnalyticsConfigurationList));

    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "VideoAnalyticsToken%d", g_onvif_cls.va_idx);
	g_onvif_cls.va_idx++;
	
	p_tmp = g_onvif_cfg.va_cfg;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.va_cfg = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API VideoAnalyticsConfigurationList * onvif_find_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList * p_head, const char * token)
{
	VideoAnalyticsConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_VideoAnalyticsConfigurations(VideoAnalyticsConfigurationList ** p_head)
{
    VideoAnalyticsConfigurationList * p_next;
	VideoAnalyticsConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_Configs(&p_tmp->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
        onvif_free_Configs(&p_tmp->Configuration.RuleEngineConfiguration.Rule);

        onvif_free_ConfigDescriptions(&p_tmp->SupportedRules.RuleDescription);
        onvif_free_ConfigDescriptions(&p_tmp->SupportedAnalyticsModules.AnalyticsModuleDescription);

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_VideoAnalyticsConfiguration()
{
    // todo : here init video analytics configurations ...
    
	ConfigList * p_config;
	ConfigDescriptionList * p_cfg_desc;
	ConfigOptionsList * p_options;
	SimpleItemList * p_simpleitem;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;
	
	VideoAnalyticsConfigurationList * p_va_cfg = onvif_add_VideoAnalyticsConfiguration();
	if (NULL == p_va_cfg)
	{
		return;
	}

	memset(p_va_cfg, 0, sizeof(VideoAnalyticsConfigurationList));

	strcpy(p_va_cfg->Configuration.Name, "VideoAnalytics");
	p_va_cfg->Configuration.UseCount = 0;
	sprintf(p_va_cfg->Configuration.token, "AnalyticsToken%d", g_onvif_cls.va_idx++);

	// todo : here init analytics engine configuration ...
	p_config = onvif_add_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
	if (p_config)
	{
		memset(p_config, 0, sizeof(ConfigList));

		strcpy(p_config->Config.Name, "MyMotionRegionDetector");
		strcpy(p_config->Config.Type, "tt:MotionRegionDetector");

		p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
		if (p_simpleitem)
		{
		    strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
		    strcpy(p_simpleitem->SimpleItem.Value, "6");
		}
	}

	// todo : here init rule engine configuration ...
	p_config = onvif_add_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule);
	if (p_config)
	{
		memset(p_config, 0, sizeof(ConfigList));

		strcpy(p_config->Config.Name, "MyMotionRegionDetector");
		strcpy(p_config->Config.Type, "tt:MotionRegionDetector");

		p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
		if (p_simpleitem)
		{
		    strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
		    strcpy(p_simpleitem->SimpleItem.Value, "6");
		}
	}

	// todo : here init supported analytics modules ...
	p_va_cfg->SupportedAnalyticsModules.sizeAnalyticsModuleContentSchemaLocation = 0;
	
	p_cfg_desc = onvif_add_ConfigDescription(&p_va_cfg->SupportedAnalyticsModules.AnalyticsModuleDescription);
	if (NULL == p_cfg_desc)
	{
	    return;
	}

    strcpy(p_cfg_desc->ConfigDescription.Name, "tt:MotionRegionDetector");

	p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
	if (p_desc)
	{
	    strcpy(p_desc->SimpleItemDescription.Name, "Sensitivity");
	    strcpy(p_desc->SimpleItemDescription.Type, "xs:integer");
	}

	p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
	if (p_message)
	{
	    p_message->Messages.IsPropertyFlag = 1;
	    p_message->Messages.IsProperty = TRUE;
	    strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/MotionRegionDetector/Motion");

	    p_message->Messages.SourceFlag = 1;
	    
	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
	        strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
	    }

	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "RuleName");
	        strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
	    }

	    p_message->Messages.DataFlag = 1;
	    
	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "State");
	        strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
	    }
	}

	p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
	if (p_options)
	{
	    strcpy(p_options->Options.RuleType, "tt:MotionRegionDetector");
	    strcpy(p_options->Options.Name, "Sensitivity");
	    strcpy(p_options->Options.Type, "xs:integer");

	    p_options->Options.any = (char *) malloc(1024);

	    strcpy(p_options->Options.any, 
	        "<tt:IntRange>"
            "<tt:Min>0</tt:Min>"
            "<tt:Max>10</tt:Max>"
            "</tt:IntRange>");
	}

	// todo : here init supported rules ...
	p_va_cfg->SupportedRules.sizeRuleContentSchemaLocation = 0;
	
	p_cfg_desc = onvif_add_ConfigDescription(&p_va_cfg->SupportedRules.RuleDescription);
	if (NULL == p_cfg_desc)
	{
	    return;
	}

    strcpy(p_cfg_desc->ConfigDescription.Name, "tt:MotionRegionDetector");

	p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
	if (p_desc)
	{
	    strcpy(p_desc->SimpleItemDescription.Name, "MotionRegion");
	    strcpy(p_desc->SimpleItemDescription.Type, "axt:MotionRegionConfig");
	}

	p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
	if (p_message)
	{
	    p_message->Messages.IsPropertyFlag = 1;
	    p_message->Messages.IsProperty = TRUE;
	    strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/MotionRegionDetector/Motion");

	    p_message->Messages.SourceFlag = 1;
	    
	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
	        strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
	    }

	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "RuleName");
	        strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
	    }

	    p_message->Messages.DataFlag = 1;
	    
	    p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
	    if (p_desc)
	    {
	        strcpy(p_desc->SimpleItemDescription.Name, "State");
	        strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
	    }
	}

	p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
	if (p_options)
	{
	    strcpy(p_options->Options.RuleType, "axt:MotionRegionConfigOptions");
	    strcpy(p_options->Options.Name, "MotionRegion");
	    strcpy(p_options->Options.Type, "axt:MotionRegionConfigOptions");

	    p_options->Options.any = (char *) malloc(1024);

	    strcpy(p_options->Options.any, 
	        "<axt:MotionRegionConfigOptions>"
	        "</axt:MotionRegionConfigOptions>");
	}
}

#endif	// end of VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT

HT_API RecordingList * onvif_add_Recording()
{
    RecordingList * p_tmp;
    RecordingList * p_new = (RecordingList *) malloc(sizeof(RecordingList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(RecordingList));
	
	snprintf(p_new->Recording.RecordingToken, ONVIF_TOKEN_LEN, "RECORDING_00%d", g_onvif_cls.recording_idx);
	g_onvif_cls.recording_idx++;

	p_tmp = g_onvif_cfg.recordings;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.recordings = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API RecordingList * onvif_find_Recording(RecordingList * p_head, const char * token)
{
	RecordingList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Recording.RecordingToken, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_Recording(RecordingList ** p_head, RecordingList * p_node)
{
    RecordingList * p_prev;
    
    p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}
	
	onvif_free_Tracks(&p_node->Recording.Tracks);

	free(p_node);
}

HT_API void onvif_free_Recordings(RecordingList ** p_head)
{
    RecordingList * p_next;
	RecordingList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_Tracks(&p_tmp->Recording.Tracks);

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API TrackList * onvif_add_Track(TrackList ** p_head)
{
	TrackList * p_tmp;
	TrackList * p_new = (TrackList *) malloc(sizeof(TrackList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(TrackList));

    snprintf(p_new->Track.TrackToken, ONVIF_TOKEN_LEN, "TRACK00%d", g_onvif_cls.track_idx);
	g_onvif_cls.track_idx++;
	
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_Track(TrackList ** p_head, TrackList * p_track)
{
    TrackList * p_prev;
    
	p_prev = *p_head;
	if (p_track == p_prev)
	{
		*p_head = p_track->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_track)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_track->next;
	}

	free(p_track);
}

HT_API void onvif_free_Tracks(TrackList ** p_head)
{
	TrackList * p_next;
	TrackList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API TrackList * onvif_find_Track(TrackList * p_head, const char * token)
{
	TrackList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Track.TrackToken, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API int	onvif_get_track_nums_by_type(TrackList * p_head, onvif_TrackType type)
{
	int nums = 0;
	
	TrackList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Track.Configuration.TrackType == type)
		{
			nums++;
		}

		p_tmp = p_tmp->next;
	}

	return nums;
}

HT_API RecordingJobList * onvif_add_RecordingJob()
{
    RecordingJobList * p_tmp;
	RecordingJobList * p_new = (RecordingJobList *) malloc(sizeof(RecordingJobList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(RecordingJobList));

	snprintf(p_new->RecordingJob.JobToken, ONVIF_TOKEN_LEN, "RECORDINGJOB_00%d", g_onvif_cls.recordingjob_idx);
	g_onvif_cls.recordingjob_idx++;

	p_tmp = g_onvif_cfg.recording_jobs;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.recording_jobs = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API RecordingJobList * onvif_find_RecordingJob(RecordingJobList * p_head, const char * token)
{
	RecordingJobList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->RecordingJob.JobToken, token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}

	return p_tmp;
}

HT_API void onvif_free_RecordingJob(RecordingJobList ** p_head, RecordingJobList * p_node)
{
    RecordingJobList * p_prev;
    
    if (NULL == p_node)
    {
        return;
    }
    
    p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

	free(p_node);
}

HT_API void onvif_free_RecordingJobs(RecordingJobList ** p_head)
{
    RecordingJobList * p_next;
	RecordingJobList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API RecordingInformationList * onvif_add_RecordingInformation(RecordingInformationList ** p_head)
{
    RecordingInformationList * p_tmp;
	RecordingInformationList * p_new = (RecordingInformationList *) malloc(sizeof(RecordingInformationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(RecordingInformationList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API void onvif_free_RecordingInformations(RecordingInformationList ** p_head)
{
    RecordingInformationList * p_next;
	RecordingInformationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API FindEventResultList * onvif_add_FindEventResult(FindEventResultList ** p_head)
{
    FindEventResultList * p_tmp;
	FindEventResultList * p_new = (FindEventResultList *) malloc(sizeof(FindEventResultList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(FindEventResultList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_FindEventResult(FindEventResultList ** p_head, FindEventResultList * p_node)
{
    FindEventResultList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

    onvif_free_SimpleItems(&p_node->Result.Event.Message.Source.SimpleItem);
    onvif_free_SimpleItems(&p_node->Result.Event.Message.Key.SimpleItem);
    onvif_free_SimpleItems(&p_node->Result.Event.Message.Data.SimpleItem);

    onvif_free_ElementItems(&p_node->Result.Event.Message.Source.ElementItem);
    onvif_free_ElementItems(&p_node->Result.Event.Message.Key.ElementItem);
    onvif_free_ElementItems(&p_node->Result.Event.Message.Data.ElementItem);
        
	free(p_node);
}

HT_API void onvif_free_FindEventResults(FindEventResultList ** p_head)
{
    FindEventResultList * p_next;
	FindEventResultList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Source.SimpleItem);
        onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Key.SimpleItem);
        onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Data.SimpleItem);

        onvif_free_ElementItems(&p_tmp->Result.Event.Message.Source.ElementItem);
        onvif_free_ElementItems(&p_tmp->Result.Event.Message.Key.ElementItem);
        onvif_free_ElementItems(&p_tmp->Result.Event.Message.Data.ElementItem);
    		
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API FindMetadataResultList * onvif_add_FindMetadataResult(FindMetadataResultList ** p_head)
{
    FindMetadataResultList * p_tmp;
	FindMetadataResultList * p_new = (FindMetadataResultList *) malloc(sizeof(FindMetadataResultList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(FindMetadataResultList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_FindMetadataResult(FindMetadataResultList ** p_head, FindMetadataResultList * p_node)
{
    FindMetadataResultList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}
    
	free(p_node);
}

HT_API void onvif_free_FindMetadataResults(FindMetadataResultList ** p_head)
{
    FindMetadataResultList * p_next;
	FindMetadataResultList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API FindPTZPositionResultList * onvif_add_FindPTZPositionResult(FindPTZPositionResultList ** p_head)
{
    FindPTZPositionResultList * p_tmp;
	FindPTZPositionResultList * p_new = (FindPTZPositionResultList *) malloc(sizeof(FindPTZPositionResultList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(FindPTZPositionResultList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_FindPTZPositionResult(FindPTZPositionResultList ** p_head, FindPTZPositionResultList * p_node)
{
    FindPTZPositionResultList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}
    
	free(p_node);
}

HT_API void onvif_free_FindPTZPositionResults(FindPTZPositionResultList ** p_head)
{
    FindPTZPositionResultList * p_next;
	FindPTZPositionResultList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_Recording()
{    
    RecordingList * p_recording = onvif_add_Recording();
    if (p_recording)
    {
        TrackList * p_track;

        strcpy(p_recording->Recording.RecordingToken, "");
        
        strcpy(p_recording->Recording.Configuration.Source.SourceId, "http://localhost/sourceID");
        strcpy(p_recording->Recording.Configuration.Source.Name, "CameraName");
        strcpy(p_recording->Recording.Configuration.Source.Location, "LocationDescription");
        strcpy(p_recording->Recording.Configuration.Source.Description, "SourceDescription");
        strcpy(p_recording->Recording.Configuration.Source.Address, "http://localhost/address");

        strcpy(p_recording->Recording.Configuration.Content, "Recording from device");
        p_recording->Recording.Configuration.MaximumRetentionTimeFlag = 1;
        p_recording->Recording.Configuration.MaximumRetentionTime = 0;

        p_recording->EarliestRecording = time(NULL) - 3600;
        p_recording->LatestRecording = time(NULL);
        
        p_track = onvif_add_Track(&p_recording->Recording.Tracks);
    	if (p_track)
    	{
    		strcpy(p_track->Track.TrackToken, "VIDEO001");
    		p_track->Track.Configuration.TrackType = TrackType_Video;

    		p_track->EarliestRecording = p_recording->EarliestRecording;
            p_track->LatestRecording = p_recording->LatestRecording;
    	}	
    	
    	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
    	if (p_track)
    	{
    		strcpy(p_track->Track.TrackToken, "AUDIO001");
    		p_track->Track.Configuration.TrackType = TrackType_Audio;

    		p_track->EarliestRecording = p_recording->EarliestRecording;
            p_track->LatestRecording = p_recording->LatestRecording;
    	}
    	
    	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
    	if (p_track)
    	{
    		strcpy(p_track->Track.TrackToken, "META001");
    		p_track->Track.Configuration.TrackType = TrackType_Metadata;

    		p_track->EarliestRecording = p_recording->EarliestRecording;
            p_track->LatestRecording = p_recording->LatestRecording;
    	}
    }
}

void onvif_init_RecordingJob()
{
    RecordingJobList * p_recordingjob = onvif_add_RecordingJob();
    if (p_recordingjob)
    {
        strcpy(p_recordingjob->RecordingJob.JobConfiguration.Mode, "Active");
        p_recordingjob->RecordingJob.JobConfiguration.Priority = 1;

        if (g_onvif_cfg.profiles)
        {
            p_recordingjob->RecordingJob.JobConfiguration.sizeSource = 1;

            p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceTokenFlag = 1;
            
            p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.TypeFlag = 1;
            strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.Type, "http://www.onvif.org/ver10/schema/Profile");
            strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.Token, g_onvif_cfg.profiles->token);

            p_recordingjob->RecordingJob.JobConfiguration.Source[0].sizeTracks = 1;
            strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].Tracks[0].SourceTag, "SourceTag");
            strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].Tracks[0].Destination, "VIDEO001");
        }
    }
}

#endif	// end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

HT_API AccessPointList * onvif_add_AccessPoint(AccessPointList ** p_head)
{
    AccessPointList * p_tmp;
	AccessPointList * p_new = (AccessPointList *) malloc(sizeof(AccessPointList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AccessPointList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API AccessPointList * onvif_find_AccessPoint(AccessPointList * p_head, const char * token)
{
    AccessPointList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->AccessPointInfo.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_AccessPoint(AccessPointList ** p_head, AccessPointList * p_node)
{
    AccessPointList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

	free(p_node);
}

HT_API void onvif_free_AccessPoints(AccessPointList ** p_head)
{
    AccessPointList * p_next;
	AccessPointList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API DoorInfoList * onvif_add_DoorInfo(DoorInfoList ** p_head)
{
    DoorInfoList * p_tmp;
	DoorInfoList * p_new = (DoorInfoList *) malloc(sizeof(DoorInfoList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(DoorInfoList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API DoorInfoList * onvif_find_DoorInfo(DoorInfoList * p_head, const char * token)
{
    DoorInfoList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcasecmp(token, p_tmp->DoorInfo.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_DoorInfos(DoorInfoList ** p_head)
{
    DoorInfoList * p_next;
	DoorInfoList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API DoorList * onvif_add_Door(DoorList ** p_head)
{
    DoorList * p_tmp;
	DoorList * p_new = (DoorList *) malloc(sizeof(DoorList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(DoorList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API DoorList * onvif_find_Door(DoorList * p_head, const char * token)
{
    DoorList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Door.DoorInfo.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_Door(DoorList ** p_head, DoorList * p_node)
{
    DoorList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

	free(p_node);
}

HT_API void onvif_free_Doors(DoorList ** p_head)
{
    DoorList * p_next;
	DoorList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AreaInfoList * onvif_add_AreaInfo(AreaInfoList ** p_head)
{
    AreaInfoList * p_tmp;
	AreaInfoList * p_new = (AreaInfoList *) malloc(sizeof(AreaInfoList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AreaInfoList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API AreaInfoList * onvif_find_AreaInfo(AreaInfoList * p_head, const char * token)
{
    AreaInfoList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->AreaInfo.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_AreaInfo(AreaInfoList ** p_head, AreaInfoList * p_node)
{
    AreaInfoList * p_prev;
    
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

	free(p_node);
}

HT_API void onvif_free_AreaInfos(AreaInfoList ** p_head)
{
    AreaInfoList * p_next;
	AreaInfoList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_AccessPoint(AccessPointList * p_accesspoint, DoorList * p_door, AreaInfoList * p_area)
{
    p_accesspoint->Enabled = TRUE;
    
    sprintf(p_accesspoint->AccessPointInfo.token, "AC_TOKEN_%d", ++g_onvif_cls.aceess_point_idx);
    sprintf(p_accesspoint->AccessPointInfo.Name, "AC_NAME_%d", g_onvif_cls.aceess_point_idx);

    p_accesspoint->AccessPointInfo.DescriptionFlag = 1;
    sprintf(p_accesspoint->AccessPointInfo.Description, "Access point %d", g_onvif_cls.aceess_point_idx);

    if (p_area)
    {
        p_accesspoint->AccessPointInfo.AreaFromFlag = 1;
        strcpy(p_accesspoint->AccessPointInfo.AreaFrom, p_area->AreaInfo.token);

        p_area = p_area->next;
    }

    if (p_area)
    {
        p_accesspoint->AccessPointInfo.AreaToFlag = 1;
        strcpy(p_accesspoint->AccessPointInfo.AreaTo, p_area->AreaInfo.token);
    }

    if (p_door)
    {
        strcpy(p_accesspoint->AccessPointInfo.Entity, p_door->Door.DoorInfo.token);

        p_accesspoint->AccessPointInfo.EntityTypeFlag = 1;
        strcpy(p_accesspoint->AccessPointInfo.EntityType, "tdc:Door");
    }

    p_accesspoint->AccessPointInfo.Capabilities.DisableAccessPoint = TRUE;
    p_accesspoint->AccessPointInfo.Capabilities.Duress = TRUE;
    p_accesspoint->AccessPointInfo.Capabilities.AnonymousAccess = TRUE;
    p_accesspoint->AccessPointInfo.Capabilities.AccessTaken = TRUE;
    p_accesspoint->AccessPointInfo.Capabilities.ExternalAuthorization = FALSE;
}

void onvif_init_AccessPointList()
{
    // here, init two access point for two door ...

    DoorList * p_door = g_onvif_cfg.doors;
    AreaInfoList * p_area = g_onvif_cfg.area_info;
    
    AccessPointList * p_accesspoint = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
    if (p_accesspoint)
    {
        onvif_init_AccessPoint(p_accesspoint, p_door, p_area);

        if (p_door)
        {
            p_door = p_door->next;
        }

        if (p_area)
        {
            p_area = p_area->next;
        }

        if (p_area)
        {
            p_area = p_area->next;
        }
    }

    p_accesspoint = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
    if (p_accesspoint)
    {
        onvif_init_AccessPoint(p_accesspoint, p_door, p_area);
    }
}

void onvif_init_Door(DoorList * p_door)
{
    sprintf(p_door->Door.DoorInfo.token, "DOOR_TOKEN_%d", ++g_onvif_cls.door_idx);
    sprintf(p_door->Door.DoorInfo.Name, "DOOR_NAME_%d", g_onvif_cls.door_idx);
    strcpy(p_door->Door.DoorType, "pt:Door");
    
    p_door->Door.DoorInfo.DescriptionFlag = 1;
    sprintf(p_door->Door.DoorInfo.Description, "Door %d", g_onvif_cls.door_idx);

    p_door->Door.DoorInfo.Capabilities.Access = TRUE;
    p_door->Door.DoorInfo.Capabilities.AccessTimingOverride = TRUE;
    p_door->Door.DoorInfo.Capabilities.Lock = TRUE;
    p_door->Door.DoorInfo.Capabilities.Unlock = TRUE;
    p_door->Door.DoorInfo.Capabilities.Block = TRUE;
    p_door->Door.DoorInfo.Capabilities.DoubleLock = TRUE;
    p_door->Door.DoorInfo.Capabilities.LockDown = TRUE;
    p_door->Door.DoorInfo.Capabilities.LockOpen = TRUE;
    p_door->Door.DoorInfo.Capabilities.DoorMonitor = TRUE;
    p_door->Door.DoorInfo.Capabilities.LockMonitor = TRUE;
    p_door->Door.DoorInfo.Capabilities.DoubleLockMonitor = TRUE;
    p_door->Door.DoorInfo.Capabilities.Alarm = TRUE;
    p_door->Door.DoorInfo.Capabilities.Tamper = TRUE;
    p_door->Door.DoorInfo.Capabilities.Fault = TRUE;

    p_door->DoorState.DoorPhysicalStateFlag = 1;
    p_door->DoorState.DoorPhysicalState = DoorPhysicalState_Closed;
    p_door->DoorState.LockPhysicalStateFlag = 1;
    p_door->DoorState.LockPhysicalState = LockPhysicalState_Locked;
    p_door->DoorState.DoubleLockPhysicalStateFlag = 1;
    p_door->DoorState.DoubleLockPhysicalState = LockPhysicalState_Locked;
    p_door->DoorState.AlarmFlag = 1;
    p_door->DoorState.Alarm = DoorAlarmState_Normal;
    p_door->DoorState.FaultFlag = 1;
    p_door->DoorState.Fault.State = DoorFaultState_NotInFault;
    p_door->DoorState.DoorMode = DoorMode_Locked;
    p_door->DoorState.TamperFlag = 1;
    p_door->DoorState.Tamper.State = DoorTamperState_NotInTamper;
}

void onvif_init_DoorList()
{
    // here, init two door ...
    
    DoorList * p_door = onvif_add_Door(&g_onvif_cfg.doors);
    if (p_door)
    {
        onvif_init_Door(p_door);
    }

    p_door = onvif_add_Door(&g_onvif_cfg.doors);
    if (p_door)
    {
        onvif_init_Door(p_door);
    }
}

void onvif_init_AreaInfo(AreaInfoList * p_info)
{
    sprintf(p_info->AreaInfo.token, "AREA_TOKEN_%d", ++g_onvif_cls.area_idx);
    sprintf(p_info->AreaInfo.Name, "AREA_NAME_%d", g_onvif_cls.area_idx);

    p_info->AreaInfo.DescriptionFlag = 1;
    sprintf(p_info->AreaInfo.Description, "Area %d", g_onvif_cls.area_idx);
}

void onvif_init_AreaInfoList()
{
    // here, init four area for two door ...
    
    AreaInfoList * p_info = onvif_add_AreaInfo(&g_onvif_cfg.area_info);
    if (p_info)
    {
        onvif_init_AreaInfo(p_info);
    }

    p_info = onvif_add_AreaInfo(&g_onvif_cfg.area_info);
    if (p_info)
    {
        onvif_init_AreaInfo(p_info);
    }

    p_info = onvif_add_AreaInfo(&g_onvif_cfg.area_info);
    if (p_info)
    {
        onvif_init_AreaInfo(p_info);
    }

    p_info = onvif_add_AreaInfo(&g_onvif_cfg.area_info);
    if (p_info)
    {
        onvif_init_AreaInfo(p_info);
    }
}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

HT_API PaneLayoutList * onvif_add_PaneLayout(PaneLayoutList ** p_head)
{
    PaneLayoutList * p_tmp;
	PaneLayoutList * p_new = (PaneLayoutList *) malloc(sizeof(PaneLayoutList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(PaneLayoutList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API PaneLayoutList * onvif_find_PaneLayout(PaneLayoutList * p_head, const char * token)
{
    PaneLayoutList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->PaneLayout.Pane) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_PaneLayouts(PaneLayoutList ** p_head)
{
    PaneLayoutList * p_next;
	PaneLayoutList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoOutputList * onvif_add_VideoOutput(VideoOutputList ** p_head)
{
    VideoOutputList * p_tmp;
	VideoOutputList * p_new = (VideoOutputList *) malloc(sizeof(VideoOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoOutputList));

    snprintf(p_new->VideoOutput.token, ONVIF_TOKEN_LEN, "VOUT_00%d", g_onvif_cls.v_out_idx);

    g_onvif_cls.v_out_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API VideoOutputList * onvif_find_VideoOutput(VideoOutputList * p_head, const char * token)
{
    VideoOutputList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->VideoOutput.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_VideoOutputs(VideoOutputList ** p_head)
{
    VideoOutputList * p_next;
	VideoOutputList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

        onvif_free_PaneLayouts(&p_tmp->VideoOutput.Layout.PaneLayout);
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API VideoOutputConfigurationList * onvif_add_VideoOutputConfiguration(VideoOutputConfigurationList ** p_head)
{
    VideoOutputConfigurationList * p_tmp;
	VideoOutputConfigurationList * p_new = (VideoOutputConfigurationList *) malloc(sizeof(VideoOutputConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoOutputConfigurationList));

    snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "VOUT_NAME_00%d", g_onvif_cls.v_out_cfg_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "VOUT_CFG_00%d", g_onvif_cls.v_out_cfg_idx);

    g_onvif_cls.v_out_cfg_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API VideoOutputConfigurationList * onvif_find_VideoOutputConfiguration(VideoOutputConfigurationList * p_head, const char * token)
{
    VideoOutputConfigurationList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Configuration.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API VideoOutputConfigurationList * onvif_find_VideoOutputConfiguration_by_OutputToken(VideoOutputConfigurationList * p_head, const char * token)
{
    VideoOutputConfigurationList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Configuration.OutputToken) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_VideoOutputConfigurations(VideoOutputConfigurationList ** p_head)
{
    VideoOutputConfigurationList * p_next;
	VideoOutputConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioOutputList * onvif_add_AudioOutput(AudioOutputList ** p_head)
{
    AudioOutputList * p_tmp;
	AudioOutputList * p_new = (AudioOutputList *) malloc(sizeof(AudioOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioOutputList));

    snprintf(p_new->AudioOutput.token, ONVIF_TOKEN_LEN, "AOUT_00%d", g_onvif_cls.a_out_idx);

    g_onvif_cls.a_out_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API AudioOutputList * onvif_find_AudioOutput(AudioOutputList * p_head, const char * token)
{
    AudioOutputList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->AudioOutput.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_AudioOutputs(AudioOutputList ** p_head)
{
    AudioOutputList * p_next;
	AudioOutputList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API AudioOutputConfigurationList * onvif_add_AudioOutputConfiguration(AudioOutputConfigurationList ** p_head)
{
    AudioOutputConfigurationList * p_tmp;
	AudioOutputConfigurationList * p_new = (AudioOutputConfigurationList *) malloc(sizeof(AudioOutputConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AudioOutputConfigurationList));

    snprintf(p_new->Configuration.Name, ONVIF_NAME_LEN, "AOUT_NAME_00%d", g_onvif_cls.a_out_cfg_idx);
    snprintf(p_new->Configuration.token, ONVIF_TOKEN_LEN, "AOUT_CFG_00%d", g_onvif_cls.a_out_cfg_idx);

    g_onvif_cls.a_out_cfg_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API AudioOutputConfigurationList * onvif_find_AudioOutputConfiguration(AudioOutputConfigurationList * p_head, const char * token)
{
    AudioOutputConfigurationList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Configuration.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API AudioOutputConfigurationList * onvif_find_AudioOutputConfiguration_by_OutputToken(AudioOutputConfigurationList * p_head, const char * token)
{
    AudioOutputConfigurationList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Configuration.OutputToken) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_AudioOutputConfigurations(AudioOutputConfigurationList ** p_head)
{
    AudioOutputConfigurationList * p_next;
	AudioOutputConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API RelayOutputList * onvif_add_RelayOutput(RelayOutputList ** p_head)
{
    RelayOutputList * p_tmp;
	RelayOutputList * p_new = (RelayOutputList *) malloc(sizeof(RelayOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(RelayOutputList));

    snprintf(p_new->RelayOutput.token, ONVIF_TOKEN_LEN, "RELAY_OUTPUT_00%d", g_onvif_cls.relay_idx);

    g_onvif_cls.relay_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API RelayOutputList * onvif_find_RelayOutput(RelayOutputList * p_head, const char * token)
{
    RelayOutputList * p_tmp = p_head;

    if (NULL == token)
    {
        return NULL;
    }
    
    while (p_tmp)
    {
        if (strcmp(token, p_tmp->RelayOutput.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_RelayOutputs(RelayOutputList ** p_head)
{
    RelayOutputList * p_next;
	RelayOutputList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API DigitalInputList * onvif_add_DigitalInput(DigitalInputList ** p_head)
{
    DigitalInputList * p_tmp;
	DigitalInputList * p_new = (DigitalInputList *) malloc(sizeof(DigitalInputList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(DigitalInputList));

    snprintf(p_new->DigitalInput.token, ONVIF_TOKEN_LEN, "DIGIT_INPUT_00%d", g_onvif_cls.digit_input_idx);

    g_onvif_cls.digit_input_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API DigitalInputList * onvif_find_DigitalInput(DigitalInputList * p_head, const char * token)
{
    DigitalInputList * p_tmp = p_head;

    if (NULL == token)
    {
        return NULL;
    }
    
    while (p_tmp)
    {
        if (strcmp(token, p_tmp->DigitalInput.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_DigitalInputs(DigitalInputList ** p_head)
{
    DigitalInputList * p_next;
	DigitalInputList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API SerialPortList * onvif_add_SerialPort(SerialPortList ** p_head)
{
    SerialPortList * p_tmp;
	SerialPortList * p_new = (SerialPortList *) malloc(sizeof(SerialPortList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(SerialPortList));

    snprintf(p_new->SerialPort.token, ONVIF_TOKEN_LEN, "SERIAL_PORT_00%d", g_onvif_cls.serial_port_idx);

    g_onvif_cls.serial_port_idx++;

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API SerialPortList * onvif_find_SerialPort(SerialPortList * p_head, const char * token)
{
    SerialPortList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->SerialPort.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API SerialPortList * onvif_find_SerialPort_by_ConfigurationToken(SerialPortList * p_head, const char * token)
{
    SerialPortList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Configuration.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_SerialPorts(SerialPortList ** p_head)
{
    SerialPortList * p_next;
	SerialPortList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API void onvif_malloc_SerialData(onvif_SerialData * p_data, int union_SerialData, int size)
{
    if (NULL == p_data)
    {
        return;
    }
    
    if (union_SerialData == 0)
    {
        p_data->_union_SerialData = 0;
        p_data->union_SerialData.Binary = (char *)malloc(size);
        memset(p_data->union_SerialData.Binary, 0, size);
    }
    else
    {
        p_data->_union_SerialData = 1;
        p_data->union_SerialData.String = (char *)malloc(size);
        memset(p_data->union_SerialData.String, 0, size);
    }
}

HT_API void onvif_free_SerialData(onvif_SerialData * p_data)
{
    if (NULL == p_data)
    {
        return;
    }

    if (p_data->_union_SerialData == 0)
    {
        if (p_data->union_SerialData.Binary)
        {
            free(p_data->union_SerialData.Binary);
            p_data->union_SerialData.Binary = NULL;
        }
    }
    else
    {
        if (p_data->union_SerialData.String)
        {
            free(p_data->union_SerialData.String);
            p_data->union_SerialData.String = NULL;
        }
    }
}

#ifdef AUDIO_SUPPORT
void onvif_init_AudioOutput()
{
    AudioOutputList * p_output = onvif_add_AudioOutput(&g_onvif_cfg.a_output);
    AudioOutputConfigurationList * p_cfg = onvif_add_AudioOutputConfiguration(&g_onvif_cfg.a_output_cfg);

    strcpy(p_cfg->Configuration.OutputToken, p_output->AudioOutput.token);
    p_cfg->Configuration.SendPrimacyFlag = 1;
    strcpy(p_cfg->Configuration.SendPrimacy, "www.onvif.org/ver20/HalfDuplex/Server");
    p_cfg->Configuration.OutputLevel = 100;

    p_cfg->Options.sizeOutputTokensAvailable = 1;
    strcpy(p_cfg->Options.OutputTokensAvailable[0], p_output->AudioOutput.token);
    p_cfg->Options.sizeSendPrimacyOptions = 3;
    strcpy(p_cfg->Options.SendPrimacyOptions[0], "www.onvif.org/ver20/HalfDuplex/Server");
    strcpy(p_cfg->Options.SendPrimacyOptions[1], "www.onvif.org/ver20/HalfDuplex/Client");
    strcpy(p_cfg->Options.SendPrimacyOptions[2], "www.onvif.org/ver20/HalfDuplex/Auto");

    p_cfg->Options.OutputLevelRange.Min = 0;
    p_cfg->Options.OutputLevelRange.Max = 100;
}
#endif

void onvif_init_RelayOutput()
{
    RelayOutputList * p_output = onvif_add_RelayOutput(&g_onvif_cfg.relay_output);

    p_output->RelayOutput.Properties.Mode = RelayMode_Monostable;
    p_output->RelayOutput.Properties.DelayTime = 10;
    p_output->RelayOutput.Properties.IdleState = RelayIdleState_closed;

    p_output->RelayLogicalState = RelayLogicalState_inactive;

    strcpy(p_output->Options.token, p_output->RelayOutput.token);
    
    p_output->Options.RelayMode_BistableFlag = 1;
    p_output->Options.RelayMode_MonostableFlag = 1;

    p_output->Options.DelayTimesFlag = 1;
    strcpy(p_output->Options.DelayTimes, "1 120");

    memcpy(&g_onvif_cfg.RelayOutputOptions, &p_output->Options, sizeof(onvif_RelayOutputOptions));
}

void onvif_init_DigitInput()
{
    DigitalInputList * p_input = onvif_add_DigitalInput(&g_onvif_cfg.digit_input);

    p_input->DigitalInput.IdleStateFlag = 1;
    p_input->DigitalInput.IdleState = DigitalIdleState_closed;

    p_input->Options.DigitalIdleState_openFlag = 1;
    p_input->Options.DigitalIdleState_closedFlag = 1;

    g_onvif_cfg.DigitalInputConfigurationOptions.DigitalIdleState_closedFlag = 1;
    g_onvif_cfg.DigitalInputConfigurationOptions.DigitalIdleState_openFlag = 1;
}

void onvif_init_SerialPort()
{
    SerialPortList * p_port = onvif_add_SerialPort(&g_onvif_cfg.serial_port);

    strcpy(p_port->Configuration.token, "SERIAL_PORT_CFG_000");
    p_port->Configuration.BaudRate = 112500;
    p_port->Configuration.CharacterLength = 8;
    p_port->Configuration.StopBit = 1;
    p_port->Configuration.ParityBit = ParityBit_Odd;
    p_port->Configuration.type = SerialPortType_RS232;

    strcpy(p_port->Options.token, p_port->SerialPort.token);
    
    p_port->Options.BaudRateList.sizeItems = 2;
    p_port->Options.BaudRateList.Items[0] = 112500;
    p_port->Options.BaudRateList.Items[1] = 98000;

    p_port->Options.CharacterLengthList.sizeItems = 1;
    p_port->Options.CharacterLengthList.Items[0] = 8;

    p_port->Options.ParityBitList.sizeItems = 2;
    p_port->Options.ParityBitList.Items[0] = ParityBit_Odd;
    p_port->Options.ParityBitList.Items[1] = ParityBit_Even;

    p_port->Options.StopBitList.sizeItems = 1;
    p_port->Options.StopBitList.Items[0] = 1;    
}

#endif // end of DEVICEIO_SUPPORT

#ifdef THERMAL_SUPPORT

HT_API ColorPaletteList * onvif_add_ColorPalette(ColorPaletteList ** p_head)
{
    ColorPaletteList * p_tmp;
	ColorPaletteList * p_new = (ColorPaletteList *) malloc(sizeof(ColorPaletteList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ColorPaletteList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_ColorPalettes(ColorPaletteList ** p_head)
{
    ColorPaletteList * p_next;
	ColorPaletteList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API NUCTableList * onvif_add_NUCTable(NUCTableList ** p_head)
{
    NUCTableList * p_tmp;
	NUCTableList * p_new = (NUCTableList *) malloc(sizeof(NUCTableList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(NUCTableList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API void onvif_free_NUCTables(NUCTableList ** p_head)
{
    NUCTableList * p_next;
	NUCTableList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_ThermalConfiguration(onvif_ThermalConfiguration * p_req)
{
    strcpy(p_req->ColorPalette.token, "CP_TOKEN");
    strcpy(p_req->ColorPalette.Name, "CP_NAME");
    strcpy(p_req->ColorPalette.Type, "WhiteHot");

    p_req->Polarity = Polarity_WhiteHot;

    p_req->NUCTableFlag = 1;
    strcpy(p_req->NUCTable.token, "NUC_TOKEN");
    strcpy(p_req->NUCTable.Name, "NUC_NAME");
    p_req->NUCTable.LowTemperatureFlag = 1;
    p_req->NUCTable.LowTemperature = 0;
    p_req->NUCTable.HighTemperatureFlag = 1;
    p_req->NUCTable.HighTemperature = 100;

    p_req->CoolerFlag = 1;
    p_req->Cooler.Enabled = TRUE;
    p_req->Cooler.RunTimeFlag = 1;
    p_req->Cooler.RunTime = 0;
}

void onvif_init_ThermalConfigurationOptions(onvif_ThermalConfigurationOptions * p_req)
{
    ColorPaletteList * p_ColorPalette1;
    ColorPaletteList * p_ColorPalette2;
    NUCTableList * p_NUCTable1;
    NUCTableList * p_NUCTable2;
    
    p_ColorPalette1 = onvif_add_ColorPalette(&p_req->ColorPalette);
    strcpy(p_ColorPalette1->ColorPalette.token, "CP_TOKEN");
    strcpy(p_ColorPalette1->ColorPalette.Name, "CP_NAME");
    strcpy(p_ColorPalette1->ColorPalette.Type, "WhiteHot");

    p_ColorPalette2 = onvif_add_ColorPalette(&p_req->ColorPalette);
    strcpy(p_ColorPalette2->ColorPalette.token, "CP_TOKEN2");
    strcpy(p_ColorPalette2->ColorPalette.Name, "CP_NAME2");
    strcpy(p_ColorPalette2->ColorPalette.Type, "BlackHot");
	    
    p_NUCTable1 = onvif_add_NUCTable(&p_req->NUCTable);
    strcpy(p_NUCTable1->NUCTable.token, "NUC_TOKEN");
    strcpy(p_NUCTable1->NUCTable.Name, "NUC_NAME");
    p_NUCTable1->NUCTable.LowTemperatureFlag = 1;
    p_NUCTable1->NUCTable.LowTemperature = 0;
    p_NUCTable1->NUCTable.HighTemperatureFlag = 1;
    p_NUCTable1->NUCTable.HighTemperature = 100;

    p_NUCTable2 = onvif_add_NUCTable(&p_req->NUCTable);
    strcpy(p_NUCTable2->NUCTable.token, "NUC_TOKEN2");
    strcpy(p_NUCTable2->NUCTable.Name, "NUC_NAME2");
    p_NUCTable2->NUCTable.LowTemperatureFlag = 1;
    p_NUCTable2->NUCTable.LowTemperature = 0;
    p_NUCTable2->NUCTable.HighTemperatureFlag = 1;
    p_NUCTable2->NUCTable.HighTemperature = 100;

    p_req->CoolerOptionsFlag = 1;
    p_req->CoolerOptions.Enabled = TRUE;
}

void onvif_init_RadiometryConfiguration(onvif_RadiometryConfiguration * p_req)
{
    p_req->RadiometryGlobalParametersFlag = 1;
    p_req->RadiometryGlobalParameters.ReflectedAmbientTemperature = 10;
    p_req->RadiometryGlobalParameters.Emissivity = 10;
    p_req->RadiometryGlobalParameters.DistanceToObject = 10;
    p_req->RadiometryGlobalParameters.RelativeHumidityFlag = 1;
    p_req->RadiometryGlobalParameters.RelativeHumidity = 10;
    p_req->RadiometryGlobalParameters.AtmosphericTemperatureFlag = 1;
    p_req->RadiometryGlobalParameters.AtmosphericTemperature = 10;
    p_req->RadiometryGlobalParameters.AtmosphericTransmittanceFlag = 1;
    p_req->RadiometryGlobalParameters.AtmosphericTransmittance = 10;
    p_req->RadiometryGlobalParameters.ExtOpticsTemperatureFlag = 1;
    p_req->RadiometryGlobalParameters.ExtOpticsTemperature = 10;
    p_req->RadiometryGlobalParameters.ExtOpticsTransmittanceFlag = 1;
    p_req->RadiometryGlobalParameters.ExtOpticsTransmittance = 10;
}

void onvif_init_RadiometryConfigurationOptions(onvif_RadiometryConfigurationOptions * p_req)
{
    p_req->RadiometryGlobalParameterOptionsFlag = 1;
    p_req->RadiometryGlobalParameterOptions.ReflectedAmbientTemperature.Min = 0;
    p_req->RadiometryGlobalParameterOptions.ReflectedAmbientTemperature.Max = 100;
    p_req->RadiometryGlobalParameterOptions.Emissivity.Min = 0;
    p_req->RadiometryGlobalParameterOptions.Emissivity.Max = 100;
    p_req->RadiometryGlobalParameterOptions.DistanceToObject.Min = 0;
    p_req->RadiometryGlobalParameterOptions.DistanceToObject.Max = 100;
    p_req->RadiometryGlobalParameterOptions.RelativeHumidityFlag = 1;
    p_req->RadiometryGlobalParameterOptions.RelativeHumidity.Min = 0;
    p_req->RadiometryGlobalParameterOptions.RelativeHumidity.Max = 100;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTemperatureFlag = 1;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTemperature.Min = 0;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTemperature.Max = 100;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittanceFlag = 1;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittance.Min = 0;
    p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittance.Max = 100;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperatureFlag = 1;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperature.Min = 0;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperature.Max = 100;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittanceFlag = 1;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittance.Min = 0;
    p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittance.Max = 100;
}

HT_API BOOL onvif_init_Thermal(VideoSourceList * p_req)
{
    onvif_init_ThermalConfiguration(&p_req->ThermalConfiguration);
    onvif_init_ThermalConfigurationOptions(&p_req->ThermalConfigurationOptions);
    onvif_init_RadiometryConfiguration(&p_req->RadiometryConfiguration);
    onvif_init_RadiometryConfigurationOptions(&p_req->RadiometryConfigurationOptions);

    return TRUE;
}

#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

HT_API CredentialList * onvif_add_Credential()
{
    CredentialList * p_tmp;
	CredentialList * p_new = (CredentialList *) malloc(sizeof(CredentialList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(CredentialList));

	p_tmp = g_onvif_cfg.credential;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.credential = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API CredentialList * onvif_find_Credential(CredentialList * p_head, const char * token)
{
    CredentialList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Credential.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_Credential(CredentialList ** p_head, CredentialList * p_node)
{
    BOOL found = FALSE;
	CredentialList * p_prev = NULL;
	CredentialList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}

		free(p_tmp);
	}
}

HT_API void onvif_free_Credentials(CredentialList ** p_head)
{
    CredentialList * p_next;
	CredentialList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API BOOL onvif_init_Credential()
{
    CredentialList * p_tmp = onvif_add_Credential();

    sprintf(p_tmp->Credential.token, "CredentialToken%d", g_onvif_cls.credential_idx++);
    p_tmp->Credential.DescriptionFlag = 1;
    strcpy(p_tmp->Credential.Description, "Credentia");
    strcpy(p_tmp->Credential.CredentialHolderReference, "testuser");

    p_tmp->Credential.sizeCredentialIdentifier = 1;
    p_tmp->Credential.CredentialIdentifier[0].Used = TRUE;
    p_tmp->Credential.CredentialIdentifier[0].ExemptedFromAuthentication = FALSE;
    strcpy(p_tmp->Credential.CredentialIdentifier[0].Type.Name, "pt:Card");
    strcpy(p_tmp->Credential.CredentialIdentifier[0].Type.FormatType, "GUID");
    strcpy(p_tmp->Credential.CredentialIdentifier[0].Value, "31343031303834323633000000000000");

#ifdef ACCESS_RULES
    if (g_onvif_cfg.access_rules)
    {
        p_tmp->Credential.sizeCredentialAccessProfile = 1;
        p_tmp->Credential.CredentialAccessProfile[0].Used = 1;
        strcpy(p_tmp->Credential.CredentialAccessProfile[0].AccessProfileToken, 
            g_onvif_cfg.access_rules->AccessProfile.token);
    }
#endif

    p_tmp->State.AntipassbackStateFlag = 1;
    
    return TRUE;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

HT_API AccessProfileList * onvif_add_AccessProfile()
{
    AccessProfileList * p_tmp;
	AccessProfileList * p_new = (AccessProfileList *) malloc(sizeof(AccessProfileList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(AccessProfileList));

	p_tmp = g_onvif_cfg.access_rules;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.access_rules = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API AccessProfileList * onvif_find_AccessProfile(AccessProfileList * p_head, const char * token)
{
    AccessProfileList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->AccessProfile.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_AccessProfile(AccessProfileList ** p_head, AccessProfileList * p_node)
{
    BOOL found = FALSE;
	AccessProfileList * p_prev = NULL;
	AccessProfileList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}

		free(p_tmp);
	}
}

HT_API void onvif_free_AccessProfiles(AccessProfileList ** p_head)
{
    AccessProfileList * p_next;
	AccessProfileList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
        
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API BOOL onvif_init_AccessProfile()
{
    AccessProfileList * p_tmp = onvif_add_AccessProfile();

    sprintf(p_tmp->AccessProfile.token, "AccessProfileToken%d", g_onvif_cls.accessrule_idx);
    sprintf(p_tmp->AccessProfile.Name, "AccessProfileName%d", g_onvif_cls.accessrule_idx++);
    p_tmp->AccessProfile.DescriptionFlag = 1;
    sprintf(p_tmp->AccessProfile.Description, "test");

    p_tmp->AccessProfile.sizeAccessPolicy = 1;
    strcpy(p_tmp->AccessProfile.AccessPolicy[0].ScheduleToken, "test");

#ifdef PROFILE_C_SUPPORT
    if (g_onvif_cfg.access_points)
    {
        strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, g_onvif_cfg.access_points->AccessPointInfo.token);
    }
    else
    {
        strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, "test");
    }
#else
    strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, "test");
#endif

    return TRUE;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

HT_API ScheduleList * onvif_add_Schedule()
{
    ScheduleList * p_tmp;
	ScheduleList * p_new = (ScheduleList *) malloc(sizeof(ScheduleList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ScheduleList));

	p_tmp = g_onvif_cfg.schedule;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.schedule= p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API ScheduleList * onvif_find_Schedule(ScheduleList * p_head, const char * token)
{
    ScheduleList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Schedule.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_Schedule(ScheduleList ** p_head, ScheduleList * p_node)
{
    BOOL found = FALSE;
	ScheduleList * p_prev = NULL;
	ScheduleList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}

#ifdef LIBICAL
        if (p_tmp->comp)
        {
            icalcomponent_free(p_tmp->comp);
        }
#endif
		free(p_tmp);
	}
}

HT_API void onvif_free_Schedules(ScheduleList ** p_head)
{
    ScheduleList * p_next;
	ScheduleList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

#ifdef LIBICAL
        if (p_tmp->comp)
        {
            icalcomponent_free(p_tmp->comp);
        }
#endif

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API BOOL onvif_init_Schedule()
{
    ScheduleList * p_tmp = onvif_add_Schedule();

    sprintf(p_tmp->Schedule.token, "ScheduleToken%d", g_onvif_cls.schedule_idx);
    sprintf(p_tmp->Schedule.Name, "ScheduleName%d", g_onvif_cls.schedule_idx++);
    p_tmp->Schedule.DescriptionFlag = 1;
    sprintf(p_tmp->Schedule.Description, "test");
    sprintf(p_tmp->Schedule.Standard, 
        "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nDTSTART:20171125T200000\r\n"
        "DTEND:20171126T020000\r\nEND:VEVENT\r\nEND:VCALENDAR");
    
    return TRUE;
}

HT_API SpecialDayGroupList * onvif_add_SpecialDayGroup()
{
    SpecialDayGroupList * p_tmp;
	SpecialDayGroupList * p_new = (SpecialDayGroupList *) malloc(sizeof(SpecialDayGroupList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(SpecialDayGroupList));

	p_tmp = g_onvif_cfg.specialdaygroup;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.specialdaygroup = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API SpecialDayGroupList * onvif_find_SpecialDayGroup(SpecialDayGroupList * p_head, const char * token)
{
    SpecialDayGroupList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->SpecialDayGroup.token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_SpecialDayGroup(SpecialDayGroupList ** p_head, SpecialDayGroupList * p_node)
{
    BOOL found = FALSE;
	SpecialDayGroupList * p_prev = NULL;
	SpecialDayGroupList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}

#ifdef LIBICAL
        if (p_tmp->comp)
        {
            icalcomponent_free(p_tmp->comp);
        }
#endif

		free(p_tmp);
	}
}

HT_API void onvif_free_SpecialDayGroups(SpecialDayGroupList ** p_head)
{
    SpecialDayGroupList * p_next;
	SpecialDayGroupList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

#ifdef LIBICAL
        if (p_tmp->comp)
        {
            icalcomponent_free(p_tmp->comp);
        }
#endif

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API BOOL onvif_init_SpecialDayGroup()
{    
    return TRUE;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

HT_API ReceiverList * onvif_add_Receiver(ReceiverList ** p_head)
{
    ReceiverList * p_tmp;
	ReceiverList * p_new = (ReceiverList *) malloc(sizeof(ReceiverList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ReceiverList));

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}	

	return p_new;
}

HT_API ReceiverList * onvif_find_Receiver(ReceiverList * p_head, const char * token)
{
    ReceiverList * p_tmp = p_head;  
    
    if (NULL == token)
    {
        return NULL;
    }

    while (p_tmp)
    {
        if (strcmp(token, p_tmp->Receiver.Token) == 0)
        {
            return p_tmp;
        }
        
        p_tmp = p_tmp->next;
    }

    return NULL;
}

HT_API void onvif_free_Receiver(ReceiverList ** p_head, ReceiverList * p_node)
{
    BOOL found = FALSE;
	ReceiverList * p_prev = NULL;
	ReceiverList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
		
		free(p_tmp);
	}
}

HT_API void onvif_free_Receivers(ReceiverList ** p_head)
{
    ReceiverList * p_next;
	ReceiverList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API int onvif_get_Receiver_nums(ReceiverList * p_head)
{
    int nums = 0;
    ReceiverList * p_tmp = p_head;

	while (p_tmp)
	{
	    nums++;
		p_tmp = p_tmp->next;
	}

	return nums;
}

#endif // end of RECEIVER_SUPPORT

#ifdef IPFILTER_SUPPORT

HT_API BOOL onvif_is_ipaddr_filter_exist(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
    int i;
	for (i = 0; i < size; i++)
	{
		if (strcmp(p_item->Address, p_head[i].Address) == 0 && p_item->PrefixLength == p_head[i].PrefixLength)
		{
			return TRUE;
		}
	}

	return FALSE;	
}

HT_API onvif_PrefixedIPAddress * onvif_find_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
    int i;
	for (i = 0; i < size; i++)
	{
		if (strcmp(p_item->Address, p_head[i].Address) == 0 && p_item->PrefixLength == p_head[i].PrefixLength)
		{
			return &p_head[i];
		}
	}

	return NULL;
}

HT_API onvif_PrefixedIPAddress * onvif_get_idle_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size)
{
    int i;
    
	for (i = 0; i < size; i++)
	{
		if (p_head[i].Address[0] == '\0')
		{
			return &p_head[i];
		}
	}

	return NULL;
}

HT_API ONVIF_RET onvif_add_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
    onvif_PrefixedIPAddress * p_ipfilter;
    
	if (onvif_is_ipaddr_filter_exist(p_head, size, p_item) == TRUE)
	{
		return ONVIF_OK;
	}

	p_ipfilter = onvif_get_idle_ipaddr_filter(p_head, size);
	if (p_ipfilter)
	{
		p_ipfilter->PrefixLength = p_item->PrefixLength;
		strcpy(p_ipfilter->Address, p_item->Address);
		
		return ONVIF_OK;
	}

	return ONVIF_ERR_IPFilterListIsFull;
}

#endif // IPFILTER_SUPPORT

HT_API void onvif_init_MulticastConfiguration(onvif_MulticastConfiguration * p_cfg)
{
	p_cfg->Port = 32002;
	p_cfg->TTL = 2;
	p_cfg->AutoStart = FALSE;
	strcpy(p_cfg->IPv4Address, "239.0.1.0");
}

/*
 * Initialize the video source
 * 
 */
void onvif_init_VideoSource()
{
	// todo : here init one video source (1280*720*25)
	
	g_onvif_cfg.v_src = (VideoSourceList *) malloc(sizeof(VideoSourceList));
	if (NULL == g_onvif_cfg.v_src)
	{
		return;
	}

	memset(g_onvif_cfg.v_src, 0, sizeof(VideoSourceList));

	g_onvif_cfg.v_src->VideoSource.Framerate = 25;
	g_onvif_cfg.v_src->VideoSource.Resolution.Width = 1280;
	g_onvif_cfg.v_src->VideoSource.Resolution.Height = 720;
	strcpy(g_onvif_cfg.v_src->VideoSource.token, "VideoSourceToken");
}

void onvif_init_VideoEncoderConfigurationOptions()
{
#ifdef MEDIA2_SUPPORT
    VideoEncoder2ConfigurationOptionsList * p_option;

    p_option = onvif_add_VideoEncoder2ConfigurationOptions(&g_onvif_cfg.v_enc_cfg_opt);

    onvif_init_VideoEncoder2ConfigurationOptions(&p_option->Options, "JPEG");

    p_option = onvif_add_VideoEncoder2ConfigurationOptions(&g_onvif_cfg.v_enc_cfg_opt);

    onvif_init_VideoEncoder2ConfigurationOptions(&p_option->Options, "MPV4-ES");

    p_option = onvif_add_VideoEncoder2ConfigurationOptions(&g_onvif_cfg.v_enc_cfg_opt);

    onvif_init_VideoEncoder2ConfigurationOptions(&p_option->Options, "H264");

    p_option = onvif_add_VideoEncoder2ConfigurationOptions(&g_onvif_cfg.v_enc_cfg_opt);

    onvif_init_VideoEncoder2ConfigurationOptions(&p_option->Options, "H265");
#endif

	// video encoder config options
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEGFlag = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4Flag = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264Flag = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.QualityRange.Min = 0;
	g_onvif_cfg.VideoEncoderConfigurationOptions.QualityRange.Max = 100;	

	// jpeg config options
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[0].Width = 1920;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[0].Height = 1080;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[1].Width = 1280;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[1].Height = 720;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[2].Width = 640;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[2].Height = 480;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[3].Width = 352;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[3].Height = 288;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[4].Width = 320;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.ResolutionsAvailable[4].Height = 240;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.FrameRateRange.Min = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.FrameRateRange.Max = 30;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.EncodingIntervalRange.Min = 5;
	g_onvif_cfg.VideoEncoderConfigurationOptions.JPEG.EncodingIntervalRange.Max = 60;

	// mpeg4 config options
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[0].Width = 1920;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[0].Height = 1080;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[1].Width = 1280;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[1].Height = 720;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[2].Width = 640;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[2].Height = 480;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[3].Width = 352;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[3].Height = 288;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[4].Width = 320;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.ResolutionsAvailable[4].Height = 240;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.Mpeg4Profile_SP = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.GovLengthRange.Min = 10;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.GovLengthRange.Max = 60;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.FrameRateRange.Min = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.FrameRateRange.Max = 30;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.EncodingIntervalRange.Min = 5;
	g_onvif_cfg.VideoEncoderConfigurationOptions.MPEG4.EncodingIntervalRange.Max = 60;

	// h264 config options
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[0].Width = 1920;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[0].Height = 1080;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[1].Width = 1280;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[1].Height = 720;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[2].Width = 640;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[2].Height = 480;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[3].Width = 352;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[3].Height = 288;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[4].Width = 320;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.ResolutionsAvailable[4].Height = 240;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.H264Profile_Baseline = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.H264Profile_Main = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.GovLengthRange.Min = 10;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.GovLengthRange.Max = 60;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.FrameRateRange.Min = 1;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.FrameRateRange.Max = 30;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.EncodingIntervalRange.Min = 5;
	g_onvif_cfg.VideoEncoderConfigurationOptions.H264.EncodingIntervalRange.Max = 60;
}

HT_API void onvif_init_VideoEncoder2ConfigurationOptions(onvif_VideoEncoder2ConfigurationOptions * p_option, const char * Encoding)
{
    strcpy(p_option->Encoding, Encoding);
    if (strcasecmp(Encoding, "JPEG") == 0)
    {
        p_option->VideoEncoding = VideoEncoding_JPEG;
    }
    else if (strcasecmp(Encoding, "MPV4-ES") == 0)
    {
        p_option->VideoEncoding = VideoEncoding_MPEG4;

        p_option->GovLengthRangeFlag = 1;
        strcpy(p_option->GovLengthRange, "1 60");

        p_option->ProfilesSupportedFlag = 1;
	    strcpy(p_option->ProfilesSupported, "Simple AdvancedSimple");
    }
    else if (strcasecmp(Encoding, "H264") == 0)
    {
        p_option->VideoEncoding = VideoEncoding_H264;

        p_option->GovLengthRangeFlag = 1;
        strcpy(p_option->GovLengthRange, "1 60");

        p_option->ProfilesSupportedFlag = 1;
	    strcpy(p_option->ProfilesSupported, "Baseline Main High");
    }
    else if (strcasecmp(Encoding, "H265") == 0)
    {
        p_option->VideoEncoding = VideoEncoding_H265;

        p_option->GovLengthRangeFlag = 1;
        strcpy(p_option->GovLengthRange, "1 60");

        p_option->ProfilesSupportedFlag = 1;
	    strcpy(p_option->ProfilesSupported, "Main Main10");
    }

    p_option->FrameRatesSupportedFlag = 1;
    sprintf(p_option->FrameRatesSupported, "30 29 25 23 20 19 18 15 12 10 8 5");

    p_option->QualityRange.Min = 0;
    p_option->QualityRange.Max = 100;

    p_option->BitrateRange.Min = 64;
    p_option->BitrateRange.Max = 4096;
    
    p_option->ResolutionsAvailable[0].Width = 1920;
	p_option->ResolutionsAvailable[0].Height = 1080;
	p_option->ResolutionsAvailable[1].Width = 1280;
	p_option->ResolutionsAvailable[1].Height = 720;
	p_option->ResolutionsAvailable[2].Width = 640;
	p_option->ResolutionsAvailable[2].Height = 480;
	p_option->ResolutionsAvailable[3].Width = 352;
	p_option->ResolutionsAvailable[3].Height = 288;
	p_option->ResolutionsAvailable[4].Width = 320;
	p_option->ResolutionsAvailable[4].Height = 240;
    
}

/*
 * Initialize the video source configuration options
 * 
 */
void onvif_init_VideoSourceConfigurationOptions()
{
	// Specify the range can be configured to  the video source
	
	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.XRange.Min = 0;
	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.XRange.Max = 100;

	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.YRange.Min = 0;
	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.YRange.Max = 100;

	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.WidthRange.Min = 320;
	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.WidthRange.Max = 1280;

	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.HeightRange.Min = 240;
	g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.HeightRange.Max = 720;	
}

void onvif_init_MetadataConfiguration()
{
    MetadataConfigurationList * p_node = onvif_add_MetadataConfiguration(&g_onvif_cfg.metadata_cfg);
	if (NULL == p_node)
	{
		return;
	}

	memset(p_node, 0, sizeof(MetadataConfigurationList));

	p_node->Configuration.AnalyticsFlag = 1;
	p_node->Configuration.Analytics = FALSE;

	p_node->Configuration.SessionTimeout = 60;
	p_node->Configuration.PTZStatusFlag = 1;
	p_node->Configuration.PTZStatus.Status = 1;
	p_node->Configuration.PTZStatus.Position = 0;

	strcpy(p_node->Configuration.Name, "MetadataConfiguration");
	strcpy(p_node->Configuration.token, "MetadataToken");

	onvif_init_MulticastConfiguration(&p_node->Configuration.Multicast);
}

void onvif_init_MetadataConfigurationOptions()
{
	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.PanTiltPositionSupportedFlag = 0;
	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.PanTiltPositionSupported = FALSE;

	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.ZoomPositionSupportedFlag = 0;
	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.ZoomPositionSupported = FALSE;

	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.PanTiltStatusSupported = TRUE;
	g_onvif_cfg.MetadataConfigurationOptions.PTZStatusFilterOptions.ZoomStatusSupported = TRUE;
}

void onvif_init_OSDConfigurationOptions()
{	
	g_onvif_cfg.OSDConfigurationOptions.OSDType_Text = 1;
	g_onvif_cfg.OSDConfigurationOptions.OSDType_Image = 0;
	g_onvif_cfg.OSDConfigurationOptions.OSDType_Extended = 0;
	g_onvif_cfg.OSDConfigurationOptions.OSDPosType_UpperLeft = 1;
	g_onvif_cfg.OSDConfigurationOptions.OSDPosType_UpperRight = 1;
	g_onvif_cfg.OSDConfigurationOptions.OSDPosType_LowerLeft = 1;
	g_onvif_cfg.OSDConfigurationOptions.OSDPosType_LowerRight = 1;
	g_onvif_cfg.OSDConfigurationOptions.OSDPosType_Custom = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOptionFlag = 1;
	g_onvif_cfg.OSDConfigurationOptions.ImageOptionFlag = 0;
	
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.ImageFlag = 0;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.PlainTextFlag = 1;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.DateFlag = 1;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.TimeFlag = 1;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.DateAndTimeFlag = 1;

	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.Total = 5;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.Image = 0;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.PlainText = 4;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.Date = 1;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.Time = 1;
	g_onvif_cfg.OSDConfigurationOptions.MaximumNumberOfOSDs.DateAndTime = 1;
	
	g_onvif_cfg.OSDConfigurationOptions.TextOption.OSDTextType_Plain = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.OSDTextType_Date = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.OSDTextType_Time = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.OSDTextType_DateAndTime = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.FontSizeRangeFlag = 1;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.FontColorFlag = 0;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.BackgroundColorFlag = 0;

	g_onvif_cfg.OSDConfigurationOptions.TextOption.FontSizeRange.Min = 16;
	g_onvif_cfg.OSDConfigurationOptions.TextOption.FontSizeRange.Max = 64;

	g_onvif_cfg.OSDConfigurationOptions.TextOption.DateFormatSize = 4;
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.DateFormat[0], "MM/dd/yyyy");
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.DateFormat[1], "dd/MM/yyyy");
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.DateFormat[2], "yyyy/MM/dd");
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.DateFormat[3], "yyyy-MM-dd");

	g_onvif_cfg.OSDConfigurationOptions.TextOption.TimeFormatSize = 2;
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.TimeFormat[0], "hh:mm:ss tt");
	strcpy(g_onvif_cfg.OSDConfigurationOptions.TextOption.TimeFormat[1], "HH:mm:ss");
}

void onvif_init_ImagingSettings()
{
	// init image setting	
	// note : Optional field flag is set to 0, this option will not appear

	g_onvif_cfg.ImagingSettings.BacklightCompensationFlag = 1;
	g_onvif_cfg.ImagingSettings.BacklightCompensation.Mode = BacklightCompensationMode_OFF;
	g_onvif_cfg.ImagingSettings.BacklightCompensation.LevelFlag = 1;
	g_onvif_cfg.ImagingSettings.BacklightCompensation.Level = 10;
	
	g_onvif_cfg.ImagingSettings.BrightnessFlag = 1;
	g_onvif_cfg.ImagingSettings.Brightness = 50;
	
	g_onvif_cfg.ImagingSettings.ColorSaturationFlag = 1;
	g_onvif_cfg.ImagingSettings.ColorSaturation = 50;
	
	g_onvif_cfg.ImagingSettings.ContrastFlag = 1;
	g_onvif_cfg.ImagingSettings.Contrast = 50;
	
	g_onvif_cfg.ImagingSettings.ExposureFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Mode = ExposureMode_AUTO;
	
	g_onvif_cfg.ImagingSettings.Exposure.PriorityFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Priority = ExposurePriority_LowNoise;

	g_onvif_cfg.ImagingSettings.Exposure.Window.bottom = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Window.top = 0;
	g_onvif_cfg.ImagingSettings.Exposure.Window.right = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Window.left = 0;
	
	g_onvif_cfg.ImagingSettings.Exposure.MinExposureTimeFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MinExposureTime = 10;
	
	g_onvif_cfg.ImagingSettings.Exposure.MaxExposureTimeFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MaxExposureTime = 40000;
	
	g_onvif_cfg.ImagingSettings.Exposure.MinGainFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MinGain = 0;
	
	g_onvif_cfg.ImagingSettings.Exposure.MaxGainFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MaxGain = 100;
	
	g_onvif_cfg.ImagingSettings.Exposure.MinIrisFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MinIris = 0;
	
	g_onvif_cfg.ImagingSettings.Exposure.MaxIrisFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.MaxIris = 10;
	
	g_onvif_cfg.ImagingSettings.Exposure.ExposureTimeFlag = 1;
    g_onvif_cfg.ImagingSettings.Exposure.ExposureTime = 4000;
    
    g_onvif_cfg.ImagingSettings.Exposure.GainFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Gain = 100;
	
	g_onvif_cfg.ImagingSettings.Exposure.IrisFlag = 1;
	g_onvif_cfg.ImagingSettings.Exposure.Iris = 10;
	
	g_onvif_cfg.ImagingSettings.FocusFlag = 1;
	g_onvif_cfg.ImagingSettings.Focus.AutoFocusMode = AutoFocusMode_AUTO;
	
	g_onvif_cfg.ImagingSettings.Focus.DefaultSpeedFlag = 1;
	g_onvif_cfg.ImagingSettings.Focus.DefaultSpeed = 100;
	
	g_onvif_cfg.ImagingSettings.Focus.NearLimitFlag = 1;
	g_onvif_cfg.ImagingSettings.Focus.NearLimit = 100;
	
	g_onvif_cfg.ImagingSettings.Focus.FarLimitFlag = 1;
	g_onvif_cfg.ImagingSettings.Focus.FarLimit = 1000;
	
	g_onvif_cfg.ImagingSettings.IrCutFilterFlag = 1;
	g_onvif_cfg.ImagingSettings.IrCutFilter = IrCutFilterMode_AUTO;
	
	g_onvif_cfg.ImagingSettings.SharpnessFlag = 1;
	g_onvif_cfg.ImagingSettings.Sharpness = 50;
	
	g_onvif_cfg.ImagingSettings.WideDynamicRangeFlag = 1;
	g_onvif_cfg.ImagingSettings.WideDynamicRange.Mode = WideDynamicMode_OFF;
	
	g_onvif_cfg.ImagingSettings.WideDynamicRange.LevelFlag = 1;
	g_onvif_cfg.ImagingSettings.WideDynamicRange.Level = 50;
	
	g_onvif_cfg.ImagingSettings.WhiteBalanceFlag = 1;
	g_onvif_cfg.ImagingSettings.WhiteBalance.Mode = WhiteBalanceMode_AUTO;
	
	g_onvif_cfg.ImagingSettings.WhiteBalance.CbGainFlag = 1;
	g_onvif_cfg.ImagingSettings.WhiteBalance.CbGain = 10;
	
	g_onvif_cfg.ImagingSettings.WhiteBalance.CrGainFlag = 1;
	g_onvif_cfg.ImagingSettings.WhiteBalance.CrGain = 10;
}

void onvif_init_ImagingOptions()
{
	// init image config options
	// note : Optional field flag is set to 0, this option will not appear

	g_onvif_cfg.ImagingOptions.BacklightCompensationFlag = 1;
	g_onvif_cfg.ImagingOptions.BacklightCompensation.Mode_OFF = 1;
	g_onvif_cfg.ImagingOptions.BacklightCompensation.Mode_ON = 1;
	g_onvif_cfg.ImagingOptions.BacklightCompensation.LevelFlag = 1;
	g_onvif_cfg.ImagingOptions.BacklightCompensation.Level.Min = 0;
	g_onvif_cfg.ImagingOptions.BacklightCompensation.Level.Max = 100;
	
	g_onvif_cfg.ImagingOptions.BrightnessFlag = 1;
	g_onvif_cfg.ImagingOptions.Brightness.Min = 0;
	g_onvif_cfg.ImagingOptions.Brightness.Max = 100;
	
	g_onvif_cfg.ImagingOptions.ColorSaturationFlag = 1;
	g_onvif_cfg.ImagingOptions.ColorSaturation.Min = 0;
	g_onvif_cfg.ImagingOptions.ColorSaturation.Max = 100;
	
	g_onvif_cfg.ImagingOptions.ContrastFlag = 1;
	g_onvif_cfg.ImagingOptions.Contrast.Min = 0;
	g_onvif_cfg.ImagingOptions.Contrast.Max = 100;
	
	g_onvif_cfg.ImagingOptions.ExposureFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Mode_AUTO = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Mode_MANUAL = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Priority_LowNoise = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Priority_FrameRate = 1;
	
	g_onvif_cfg.ImagingOptions.Exposure.MinExposureTimeFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MinExposureTime.Min = 10;
	g_onvif_cfg.ImagingOptions.Exposure.MinExposureTime.Max = 10;
	
	g_onvif_cfg.ImagingOptions.Exposure.MaxExposureTimeFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MaxExposureTime.Min = 10;
	g_onvif_cfg.ImagingOptions.Exposure.MaxExposureTime.Max = 320000;

	g_onvif_cfg.ImagingOptions.Exposure.MinGainFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MinGain.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.MinGain.Max = 0;

	g_onvif_cfg.ImagingOptions.Exposure.MaxGainFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MaxGain.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.MaxGain.Max = 100;

	g_onvif_cfg.ImagingOptions.Exposure.MinIrisFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MinIris.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.MinIris.Max = 10;

	g_onvif_cfg.ImagingOptions.Exposure.MaxIrisFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.MaxIris.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.MaxIris.Max = 10;

	g_onvif_cfg.ImagingOptions.Exposure.ExposureTimeFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.ExposureTime.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.ExposureTime.Max = 40000;

	g_onvif_cfg.ImagingOptions.Exposure.GainFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Gain.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.Gain.Max = 100;

	g_onvif_cfg.ImagingOptions.Exposure.IrisFlag = 1;
	g_onvif_cfg.ImagingOptions.Exposure.Iris.Min = 0;
	g_onvif_cfg.ImagingOptions.Exposure.Iris.Max = 100;

	g_onvif_cfg.ImagingOptions.FocusFlag = 1;
    g_onvif_cfg.ImagingOptions.Focus.AutoFocusModes_AUTO = 1;
    g_onvif_cfg.ImagingOptions.Focus.AutoFocusModes_MANUAL = 1;

    g_onvif_cfg.ImagingOptions.Focus.DefaultSpeedFlag = 1;
	g_onvif_cfg.ImagingOptions.Focus.DefaultSpeed.Min = 0;
	g_onvif_cfg.ImagingOptions.Focus.DefaultSpeed.Max = 100;

	g_onvif_cfg.ImagingOptions.Focus.NearLimitFlag = 1;
	g_onvif_cfg.ImagingOptions.Focus.NearLimit.Min = 0;
	g_onvif_cfg.ImagingOptions.Focus.NearLimit.Max = 100;

	g_onvif_cfg.ImagingOptions.Focus.FarLimitFlag = 1;
	g_onvif_cfg.ImagingOptions.Focus.FarLimit.Min = 0;
	g_onvif_cfg.ImagingOptions.Focus.FarLimit.Max = 1000;	
	
	g_onvif_cfg.ImagingOptions.IrCutFilterMode_ON = 1;
	g_onvif_cfg.ImagingOptions.IrCutFilterMode_OFF = 1;
	g_onvif_cfg.ImagingOptions.IrCutFilterMode_AUTO = 1;

	g_onvif_cfg.ImagingOptions.SharpnessFlag = 1;
	g_onvif_cfg.ImagingOptions.Sharpness.Min = 0;
	g_onvif_cfg.ImagingOptions.Sharpness.Max = 100;

	g_onvif_cfg.ImagingOptions.WideDynamicRangeFlag = 1;	
	g_onvif_cfg.ImagingOptions.WideDynamicRange.Mode_OFF = 1;
	g_onvif_cfg.ImagingOptions.WideDynamicRange.Mode_ON = 1;

	g_onvif_cfg.ImagingOptions.WideDynamicRange.LevelFlag = 1;
	g_onvif_cfg.ImagingOptions.WideDynamicRange.Level.Min = 0;
	g_onvif_cfg.ImagingOptions.WideDynamicRange.Level.Max = 100;

	g_onvif_cfg.ImagingOptions.WhiteBalanceFlag = 1;
	g_onvif_cfg.ImagingOptions.WhiteBalance.Mode_AUTO = 1;
	g_onvif_cfg.ImagingOptions.WhiteBalance.Mode_MANUAL = 1;

	g_onvif_cfg.ImagingOptions.WhiteBalance.YrGainFlag = 1;
    g_onvif_cfg.ImagingOptions.WhiteBalance.YrGain.Min = 0;
	g_onvif_cfg.ImagingOptions.WhiteBalance.YrGain.Max = 100;

	g_onvif_cfg.ImagingOptions.WhiteBalance.YbGainFlag = 1;
	g_onvif_cfg.ImagingOptions.WhiteBalance.YbGain.Min = 0;
	g_onvif_cfg.ImagingOptions.WhiteBalance.YbGain.Max = 100;
}

void onvif_init_xaddr(char * addr, int addrlen, const char * suffix)
{
	int i;
    BOOL first = TRUE;
    int offset = 0;

    for (i = 0; i < g_onvif_cfg.servs_num; i++)
    {
        if (first)
	    {
	        first = FALSE;
	    }
	    else
	    {
	        offset += snprintf(addr+offset, addrlen-offset, " ");
	    }
    	    
#ifdef HTTPS
    	if (g_onvif_cfg.https_enable)
    	{
        	offset += snprintf(addr+offset, addrlen-offset,  
        	    "https://%s:%d%s", g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port, suffix);
        }
        else
        {
            offset += snprintf(addr+offset, addrlen-offset,  
        	    "http://%s:%d%s", g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port, suffix);
        }
#else
    	offset += snprintf(addr+offset, addrlen-offset,  
    	    "http://%s:%d%s", g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port, suffix);
#endif
    }
}

void onvif_init_capabilities()
{
#ifdef DEVICEIO_SUPPORT
    int vsrc = 0, asrc = 0, vout = 0, aout = 0;
    int relay_output = 0, serial_port = 0, digit_input = 0;
    VideoSourceList    * p_vsrc;
    VideoOutputList    * p_vout;
    RelayOutputList    * p_relay_output;
	SerialPortList     * p_serial_port;
	DigitalInputList   * p_digital_input;	
#ifdef AUDIO_SUPPORT    
    AudioSourceList    * p_asrc;
    AudioOutputList    * p_aout;
#endif    
#endif

	// network capabilities

#ifdef IPFILTER_SUPPORT	
	g_onvif_cfg.Capabilities.device.IPFilter = 1;
#else
    g_onvif_cfg.Capabilities.device.IPFilter = 0;
#endif
	g_onvif_cfg.Capabilities.device.ZeroConfiguration = 1;
	g_onvif_cfg.Capabilities.device.IPVersion6 = 0;
	g_onvif_cfg.Capabilities.device.DynDNS = 1;
	g_onvif_cfg.Capabilities.device.Dot11Configuration = 1;
	g_onvif_cfg.Capabilities.device.HostnameFromDHCP = 1;
	g_onvif_cfg.Capabilities.device.DHCPv6 = 0;

	// system capabilities
	g_onvif_cfg.Capabilities.device.DiscoveryResolve = 1;
	g_onvif_cfg.Capabilities.device.DiscoveryBye = 1;
	g_onvif_cfg.Capabilities.device.RemoteDiscovery = 0;
	g_onvif_cfg.Capabilities.device.SystemBackup = 1;
	g_onvif_cfg.Capabilities.device.SystemLogging = 1;
	g_onvif_cfg.Capabilities.device.FirmwareUpgrade = 1;
	g_onvif_cfg.Capabilities.device.HttpFirmwareUpgrade = 1;
	g_onvif_cfg.Capabilities.device.HttpSystemBackup = 1;
	g_onvif_cfg.Capabilities.device.HttpSystemLogging = 1;
	g_onvif_cfg.Capabilities.device.HttpSupportInformation = 1;
	g_onvif_cfg.Capabilities.device.StorageConfiguration = 0;

    // scurity capabilities
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		g_onvif_cfg.Capabilities.device.TLS10 = 1;
    	g_onvif_cfg.Capabilities.device.TLS11 = 1;
    	g_onvif_cfg.Capabilities.device.TLS12 = 1;
    }
    else
    {
    	g_onvif_cfg.Capabilities.device.TLS10 = 0;
    	g_onvif_cfg.Capabilities.device.TLS11 = 0;
    	g_onvif_cfg.Capabilities.device.TLS12 = 0;
    }
#else
	g_onvif_cfg.Capabilities.device.TLS10 = 0;
	g_onvif_cfg.Capabilities.device.TLS11 = 0;
	g_onvif_cfg.Capabilities.device.TLS12 = 0;
#endif
    g_onvif_cfg.Capabilities.device.OnboardKeyGeneration = 0;
    g_onvif_cfg.Capabilities.device.AccessPolicyConfig = 1;
    g_onvif_cfg.Capabilities.device.DefaultAccessPolicy = 1;
    g_onvif_cfg.Capabilities.device.Dot1X = 1;
    g_onvif_cfg.Capabilities.device.RemoteUserHandling = 1;
    g_onvif_cfg.Capabilities.device.X509Token = 0;
    g_onvif_cfg.Capabilities.device.SAMLToken = 0;
    g_onvif_cfg.Capabilities.device.KerberosToken = 0;
    g_onvif_cfg.Capabilities.device.UsernameToken = 1;
    g_onvif_cfg.Capabilities.device.HttpDigest = 1;
    g_onvif_cfg.Capabilities.device.RELToken = 0;

    g_onvif_cfg.Capabilities.device.Dot1XConfigurations = 1;
	g_onvif_cfg.Capabilities.device.NTP = MAX_NTP_SERVER;
	g_onvif_cfg.Capabilities.device.SupportedEAPMethods = 0;
	g_onvif_cfg.Capabilities.device.MaxUsers = MAX_USERS;
	g_onvif_cfg.Capabilities.device.MaxUserNameLength = 32;
	g_onvif_cfg.Capabilities.device.MaxPasswordLength = 32;

#ifdef DEVICEIO_SUPPORT
    g_onvif_cfg.Capabilities.device.InputConnectors = 0;
    g_onvif_cfg.Capabilities.device.RelayOutputs = 1;
#endif

    onvif_init_xaddr(g_onvif_cfg.Capabilities.device.XAddr, XADDR_LEN-1, "/onvif/device_service");

#ifdef MEDIA_SUPPORT
	// media capabilities
    g_onvif_cfg.Capabilities.media.SnapshotUri = 1;
    g_onvif_cfg.Capabilities.media.Rotation = 0;
    g_onvif_cfg.Capabilities.media.VideoSourceMode = 1;
    g_onvif_cfg.Capabilities.media.OSD = 1;
    g_onvif_cfg.Capabilities.media.EXICompression = 0;
	g_onvif_cfg.Capabilities.media.RTPMulticast = 0;
	g_onvif_cfg.Capabilities.media.RTP_TCP = 1;
	g_onvif_cfg.Capabilities.media.RTP_RTSP_TCP = 1;
	g_onvif_cfg.Capabilities.media.NonAggregateControl = 0;
	g_onvif_cfg.Capabilities.media.NoRTSPStreaming = 0;
	g_onvif_cfg.Capabilities.media.support = 1;

	g_onvif_cfg.Capabilities.media.MaximumNumberOfProfiles = 10;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.media.XAddr, XADDR_LEN-1, "/onvif/media_service");
#endif // MEDIA_SUPPORT

#ifdef MEDIA2_SUPPORT
    // media2 capabilities
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTP_RTSP_TCPFlag = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTP_RTSP_TCP = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTSPStreamingFlag = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTSPStreaming = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTPMulticastFlag = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTPMulticast = 1;
    g_onvif_cfg.Capabilities.media2.StreamingCapabilities.AutoStartMulticast = 0;
    g_onvif_cfg.Capabilities.media2.SnapshotUriFlag = 1;
    g_onvif_cfg.Capabilities.media2.SnapshotUri = 1;
    g_onvif_cfg.Capabilities.media2.Rotation = 0;
    g_onvif_cfg.Capabilities.media2.VideoSourceMode = 1;
    g_onvif_cfg.Capabilities.media2.OSDFlag = 1;
    g_onvif_cfg.Capabilities.media2.OSD = 1;
    g_onvif_cfg.Capabilities.media2.TemporaryOSDText = 1;
    g_onvif_cfg.Capabilities.media2.Mask = 1;
    g_onvif_cfg.Capabilities.media2.SourceMask = 1;
    g_onvif_cfg.Capabilities.media2.ProfileCapabilities.MaximumNumberOfProfilesFlag = 1;
    g_onvif_cfg.Capabilities.media2.ProfileCapabilities.MaximumNumberOfProfiles = 10;
    g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupportedFlag = 1;

    strcpy(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, "VideoSource VideoEncoder Metadata");

#ifdef PTZ_SUPPORT
    strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " PTZ");
#endif
#ifdef AUDIO_SUPPORT
    strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " AudioSource AudioEncoder");
#ifdef DEVICEIO_SUPPORT
    strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " AudioOutput AudioDecoder");
#endif
#endif

#ifdef VIDEO_ANALYTICS
    strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " Analytics");
#endif

	g_onvif_cfg.Capabilities.media2.support = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.media2.XAddr, XADDR_LEN-1, "/onvif/media2_service");

#endif // end of MEDIA2_SUPPORT

	// event capabilities
    g_onvif_cfg.Capabilities.events.WSSubscriptionPolicySupport = 1;
	g_onvif_cfg.Capabilities.events.WSPullPointSupport = 1;
	g_onvif_cfg.Capabilities.events.WSPausableSubscriptionManagerInterfaceSupport = 0;
	g_onvif_cfg.Capabilities.events.PersistentNotificationStorage = 0;
	g_onvif_cfg.Capabilities.events.support = 1;

	g_onvif_cfg.Capabilities.events.MaxNotificationProducers = 10;
	g_onvif_cfg.Capabilities.events.MaxPullPoints = 10;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.events.XAddr, XADDR_LEN-1, "/onvif/event_service");

#ifdef IMAGE_SUPPORT
	// image capabilities
    g_onvif_cfg.Capabilities.image.ImageStabilization = 0;
    g_onvif_cfg.Capabilities.image.Presets = 1;
    g_onvif_cfg.Capabilities.image.AdaptablePreset = 1;
	g_onvif_cfg.Capabilities.image.support = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.image.XAddr, XADDR_LEN-1, "/onvif/image_service");
#endif // IMAGE_SUPPORT

#ifdef PTZ_SUPPORT
	// ptz capabilities
	g_onvif_cfg.Capabilities.ptz.EFlip = 1;
    g_onvif_cfg.Capabilities.ptz.Reverse = 1;
    g_onvif_cfg.Capabilities.ptz.GetCompatibleConfigurations = 1;
    g_onvif_cfg.Capabilities.ptz.MoveStatus = 1;
    g_onvif_cfg.Capabilities.ptz.StatusPosition = 1;
    g_onvif_cfg.Capabilities.ptz.support = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.ptz.XAddr, XADDR_LEN-1, "/onvif/ptz_service");

#endif // end of PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
	// analytics capabilities
	g_onvif_cfg.Capabilities.analytics.RuleSupport = 1;
	g_onvif_cfg.Capabilities.analytics.AnalyticsModuleSupport = 1;
	g_onvif_cfg.Capabilities.analytics.CellBasedSceneDescriptionSupported = 0;
	g_onvif_cfg.Capabilities.analytics.support = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.analytics.XAddr, XADDR_LEN-1, "/onvif/analytics_service");
#endif // end of VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT
	// record capabilities
    g_onvif_cfg.Capabilities.recording.ReceiverSource = 0;
    g_onvif_cfg.Capabilities.recording.MediaProfileSource = 1;
    g_onvif_cfg.Capabilities.recording.DynamicRecordings = 1;
    g_onvif_cfg.Capabilities.recording.DynamicTracks = 1;
    g_onvif_cfg.Capabilities.recording.Options = 1;
    g_onvif_cfg.Capabilities.recording.MetadataRecording = 1;
    g_onvif_cfg.Capabilities.recording.JPEG = 1;
    g_onvif_cfg.Capabilities.recording.MPEG4 = 1;
    g_onvif_cfg.Capabilities.recording.H264 = 1;
#ifdef AUDIO_SUPPORT    
    g_onvif_cfg.Capabilities.recording.G711 = 1;
    g_onvif_cfg.Capabilities.recording.G726 = 1;
    g_onvif_cfg.Capabilities.recording.AAC = 1;
#else
    g_onvif_cfg.Capabilities.recording.G711 = 0;
    g_onvif_cfg.Capabilities.recording.G726 = 0;
    g_onvif_cfg.Capabilities.recording.AAC = 0;
#endif
    g_onvif_cfg.Capabilities.recording.support = 1;

	g_onvif_cfg.Capabilities.recording.MaxStringLength = 256;
	g_onvif_cfg.Capabilities.recording.MaxRate = 200;
	g_onvif_cfg.Capabilities.recording.MaxTotalRate = 2000;
    g_onvif_cfg.Capabilities.recording.MaxRecordings = 5;
    g_onvif_cfg.Capabilities.recording.MaxRecordingJobs = 5;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.recording.XAddr, XADDR_LEN-1, "/onvif/recording_service");

	// search capabilities
	g_onvif_cfg.Capabilities.search.MetadataSearch = 1;
    g_onvif_cfg.Capabilities.search.GeneralStartEvents = 1;
    g_onvif_cfg.Capabilities.search.support = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.search.XAddr, XADDR_LEN-1, "/onvif/search_service");

	// replay capabilities
	g_onvif_cfg.Capabilities.replay.ReversePlayback = 0;
    g_onvif_cfg.Capabilities.replay.RTP_RTSP_TCP = 1;
    g_onvif_cfg.Capabilities.replay.support = 1;

	g_onvif_cfg.Capabilities.replay.SessionTimeoutRange.Min = 10;
	g_onvif_cfg.Capabilities.replay.SessionTimeoutRange.Max = 100;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.replay.XAddr, XADDR_LEN-1, "/onvif/replay_service");
#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
    // accesscontrol capabilities
    g_onvif_cfg.Capabilities.accesscontrol.support = 1;
    g_onvif_cfg.Capabilities.accesscontrol.MaxLimit = ACCESS_CTRL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.accesscontrol.MaxAccessPoints = ACCESS_CTRL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.accesscontrol.MaxAreas = ACCESS_CTRL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.accesscontrol.ClientSuppliedTokenSupported = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.accesscontrol.XAddr, XADDR_LEN-1, "/onvif/accesscontrol_service");

    // doorcontrol capabilities
    g_onvif_cfg.Capabilities.doorcontrol.support = 1;
    g_onvif_cfg.Capabilities.doorcontrol.MaxLimit = DOOR_CTRL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.doorcontrol.MaxDoors = DOOR_CTRL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.doorcontrol.ClientSuppliedTokenSupported = 1;
    g_onvif_cfg.Capabilities.doorcontrol.DoorManagementSupported = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.doorcontrol.XAddr, XADDR_LEN-1, "/onvif/doorcontrol_service");
#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT
    p_vsrc = g_onvif_cfg.v_src;
    while (p_vsrc)
    {
        vsrc++;

        p_vsrc = p_vsrc->next;
    }

    p_vout = g_onvif_cfg.v_output;
    while (p_vout)
    {
        vout++;

        p_vout = p_vout->next;
    }

#ifdef AUDIO_SUPPORT
    p_asrc = g_onvif_cfg.a_src;
    while (p_asrc)
    {
        asrc++;

        p_asrc = p_asrc->next;
    }

    p_aout = g_onvif_cfg.a_output;
    while (p_aout)
    {
        aout++;

        p_aout = p_aout->next;
    }
#endif

    p_relay_output = g_onvif_cfg.relay_output;
    while (p_relay_output)
    {
        relay_output++;

        p_relay_output = p_relay_output->next;
    }

    p_serial_port = g_onvif_cfg.serial_port;
    while (p_serial_port)
    {
        serial_port++;

        p_serial_port = p_serial_port->next;
    }

    p_digital_input = g_onvif_cfg.digit_input;
    while (p_digital_input)
    {
        digit_input++;

        p_digital_input = p_digital_input->next;
    }
    
    // deviceIO capabilities
    g_onvif_cfg.Capabilities.deviceIO.support = 1;
    g_onvif_cfg.Capabilities.deviceIO.VideoSourcesFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.VideoSources = vsrc;
    g_onvif_cfg.Capabilities.deviceIO.VideoOutputsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.VideoOutputs = vout;
#ifdef AUDIO_SUPPORT    
    g_onvif_cfg.Capabilities.deviceIO.AudioSourcesFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.AudioSources = asrc;
    g_onvif_cfg.Capabilities.deviceIO.AudioOutputsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.AudioOutputs = aout;
#endif    
    g_onvif_cfg.Capabilities.deviceIO.RelayOutputsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.RelayOutputs = relay_output;
    g_onvif_cfg.Capabilities.deviceIO.SerialPortsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.SerialPorts = serial_port;
    g_onvif_cfg.Capabilities.deviceIO.DigitalInputsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.DigitalInputs = digit_input;
    g_onvif_cfg.Capabilities.deviceIO.DigitalInputOptionsFlag = 1;
    g_onvif_cfg.Capabilities.deviceIO.DigitalInputOptions = TRUE;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.deviceIO.XAddr, XADDR_LEN-1, "/onvif/deviceIO_service");
#endif // end of DEVICEIO_SUPPORT

    // dot11 capabilities
    g_onvif_cfg.Capabilities.dot11.TKIP = 1;
    g_onvif_cfg.Capabilities.dot11.ScanAvailableNetworks = 1;
    g_onvif_cfg.Capabilities.dot11.MultipleConfiguration = 0;
    g_onvif_cfg.Capabilities.dot11.AdHocStationMode = 1;
    g_onvif_cfg.Capabilities.dot11.WEP = 1;

#ifdef THERMAL_SUPPORT
    // thermal capabilities
    g_onvif_cfg.Capabilities.thermal.support = 1;
    g_onvif_cfg.Capabilities.thermal.Radiometry = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.thermal.XAddr, XADDR_LEN-1, "/onvif/thermal_service");
#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
    // credential capabilities
    g_onvif_cfg.Capabilities.credential.support = 1;
    g_onvif_cfg.Capabilities.credential.CredentialValiditySupported = 1;
    g_onvif_cfg.Capabilities.credential.CredentialAccessProfileValiditySupported = 1;
    g_onvif_cfg.Capabilities.credential.ValiditySupportsTimeValue = 1;
    g_onvif_cfg.Capabilities.credential.ResetAntipassbackSupported = 1;
    g_onvif_cfg.Capabilities.credential.ClientSuppliedTokenSupported = 1;
    g_onvif_cfg.Capabilities.credential.MaxLimit = CREDENTIAL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.credential.MaxCredentials = CREDENTIAL_MAX_LIMIT;
    g_onvif_cfg.Capabilities.credential.MaxAccessProfilesPerCredential = CREDENTIAL_MAX_LIMIT;

    g_onvif_cfg.Capabilities.credential.sizeSupportedIdentifierType = 3;
    strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[0], "pt:Card");
    strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[1], "pt:PIN");
    strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[2], "pt:Fingerprint");

    strcpy(g_onvif_cfg.Capabilities.credential.DefaultCredentialSuspensionDuration, "PT5M");

    g_onvif_cfg.Capabilities.credential.ExtensionFlag = 0;
    g_onvif_cfg.Capabilities.credential.Extension.sizeSupportedExemptionType = 0;
    
    onvif_init_xaddr(g_onvif_cfg.Capabilities.credential.XAddr, XADDR_LEN-1, "/onvif/credential_service");
#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
    // access rules capabilities
    g_onvif_cfg.Capabilities.accessrules.support = 1;
    g_onvif_cfg.Capabilities.accessrules.MaxLimit = ACCESSRULES_MAX_LIMIT;
    g_onvif_cfg.Capabilities.accessrules.MaxAccessProfiles = ACCESSRULES_MAX_LIMIT;
    g_onvif_cfg.Capabilities.accessrules.MaxAccessPoliciesPerAccessProfile = 1;
    g_onvif_cfg.Capabilities.accessrules.MultipleSchedulesPerAccessPointSupported = 1;
    g_onvif_cfg.Capabilities.accessrules.ClientSuppliedTokenSupported = 1;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.accessrules.XAddr, XADDR_LEN-1, "/onvif/accessrules_service");
#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
    g_onvif_cfg.Capabilities.schedule.support = 1;
    g_onvif_cfg.Capabilities.schedule.MaxLimit = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.MaxSchedules = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.MaxTimePeriodsPerDay = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.MaxSpecialDayGroups = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.MaxDaysInSpecialDayGroup = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.MaxSpecialDaysSchedules = SCHEDULE_MAX_LIMIT;
    g_onvif_cfg.Capabilities.schedule.ExtendedRecurrenceSupported = 1;
    g_onvif_cfg.Capabilities.schedule.SpecialDaysSupported = 1;
    g_onvif_cfg.Capabilities.schedule.StateReportingSupported = 0;
    g_onvif_cfg.Capabilities.schedule.ClientSuppliedTokenSupported = 0;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.schedule.XAddr, XADDR_LEN-1, "/onvif/schedule_service");
#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
    g_onvif_cfg.Capabilities.receiver.support = 1;
    g_onvif_cfg.Capabilities.receiver.RTP_USCOREMulticast = 1;
    g_onvif_cfg.Capabilities.receiver.RTP_USCORETCP = 1;
    g_onvif_cfg.Capabilities.receiver.RTP_USCORERTSP_USCORETCP = 1;

    g_onvif_cfg.Capabilities.receiver.SupportedReceivers = 10;
    g_onvif_cfg.Capabilities.receiver.MaximumRTSPURILength = 256;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.receiver.XAddr, XADDR_LEN-1, "/onvif/receiver_service");
#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT
    g_onvif_cfg.Capabilities.provisioning.support = 1;
    g_onvif_cfg.Capabilities.provisioning.DefaultTimeout = 60;
    g_onvif_cfg.Capabilities.provisioning.sizeSource = 1;

    if (g_onvif_cfg.v_src)
    {
        strcpy(g_onvif_cfg.Capabilities.provisioning.Source[0].VideoSourceToken, g_onvif_cfg.v_src->VideoSource.token);
    }

    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumPanMovesFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumPanMoves = 60;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumTiltMovesFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumTiltMoves = 60;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumZoomMovesFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumZoomMoves = 60;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumRollMovesFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumRollMoves = 60;
    g_onvif_cfg.Capabilities.provisioning.Source[0].AutoLevelFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].AutoLevel = TRUE;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumFocusMovesFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumFocusMoves = 60;
    g_onvif_cfg.Capabilities.provisioning.Source[0].AutoFocusFlag = 1;
    g_onvif_cfg.Capabilities.provisioning.Source[0].AutoFocus = TRUE;

    onvif_init_xaddr(g_onvif_cfg.Capabilities.provisioning.XAddr, XADDR_LEN-1, "/onvif/provisioning_service");
#endif // end of PROVISIONING_SUPPORT
}

void onvif_init_SystemDateTime()
{
	g_onvif_cfg.SystemDateTime.DateTimeType = SetDateTimeType_NTP;
    g_onvif_cfg.SystemDateTime.DaylightSavings = FALSE;

    g_onvif_cfg.SystemDateTime.TimeZoneFlag = 1;
    
	onvif_get_timezone(g_onvif_cfg.SystemDateTime.TimeZone.TZ, sizeof(g_onvif_cfg.SystemDateTime.TimeZone.TZ));	
}

void onvif_init_NetworkInterface()
{
#if __WINDOWS_OS__
    
    int trycnt = 0;
    DWORD ret;
    DWORD outBufLen = 15000;
    IP_ADAPTER_ADDRESSES * paddrs = NULL;
    IP_ADAPTER_ADDRESSES * pcuraddr = NULL;
    
    do {
    
        paddrs = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);
        if (paddrs == NULL) 
        {
            log_print(HT_LOG_ERR, "%s, memory allocation failed\r\n", __FUNCTION__);
            return;
        }

        ret = GetAdaptersAddresses(AF_INET, 0, 0, paddrs, &outBufLen);

        if (ret == ERROR_BUFFER_OVERFLOW) 
        {
            free(paddrs);
            paddrs = NULL;
        } 
        else 
        {
            break;
        }

        trycnt++;

    } while ((ret == ERROR_BUFFER_OVERFLOW) && (trycnt < 3));

    if (NO_ERROR == ret)
    {
        pcuraddr = paddrs;
        
        while (pcuraddr)
        {
            NetworkInterfaceList * p_net_inf;
            IP_ADAPTER_UNICAST_ADDRESS * p_ipaddr;
            
            if (pcuraddr->IfType & IF_TYPE_SOFTWARE_LOOPBACK)
            {
                pcuraddr = pcuraddr->Next;
                continue;
            }

            // not 169.xxx.xxx.xxx
            p_ipaddr = pcuraddr->FirstUnicastAddress;
            while (p_ipaddr)
            {
                if (p_ipaddr->Address.lpSockaddr->sa_family != AF_INET)
                {
                    goto NEXT;
                }
                else
                {
                    struct sockaddr_in * p_inaddr = (struct sockaddr_in *)p_ipaddr->Address.lpSockaddr;

                    if (p_inaddr->sin_addr.s_net == 169)
                    {
                        goto NEXT;
                    }
                }

                p_ipaddr = p_ipaddr->Next;
            }
            
            p_net_inf = onvif_add_NetworkInterface();
            if (NULL == p_net_inf)
            {
                if (paddrs) 
                {
                    free(paddrs);
                }
                
                return;
            }

            p_net_inf->NetworkInterface.Enabled = TRUE;
            p_net_inf->NetworkInterface.InfoFlag = 1;           
            p_net_inf->NetworkInterface.Info.MTUFlag = 1;
            p_net_inf->NetworkInterface.IPv4Flag = 1;           
            
            sprintf(p_net_inf->NetworkInterface.Info.HwAddress, "%02X-%02X-%02X-%02X-%02X-%02X", 
                pcuraddr->PhysicalAddress[0], pcuraddr->PhysicalAddress[1], pcuraddr->PhysicalAddress[2], 
                pcuraddr->PhysicalAddress[3], pcuraddr->PhysicalAddress[4], pcuraddr->PhysicalAddress[5]);

            p_net_inf->NetworkInterface.Info.MTU = pcuraddr->Mtu;
            p_net_inf->NetworkInterface.IPv4.Enabled = (pcuraddr->Flags & IP_ADAPTER_IPV4_ENABLED);
            p_net_inf->NetworkInterface.IPv4.Config.DHCP = (pcuraddr->Flags & IP_ADAPTER_DHCP_ENABLED);

            p_ipaddr = pcuraddr->FirstUnicastAddress;
            while (p_ipaddr)
            {
                if (p_ipaddr->Address.lpSockaddr->sa_family == AF_INET)
                {
                    struct sockaddr_in * p_inaddr = (struct sockaddr_in *)p_ipaddr->Address.lpSockaddr;

                    strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, inet_ntoa(p_inaddr->sin_addr));
                    if (p_ipaddr->OnLinkPrefixLength > 0 && p_ipaddr->OnLinkPrefixLength <= 32)
                    {
                        p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = p_ipaddr->OnLinkPrefixLength;
                    }
                    else
                    {
                        p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = 24;
                    }
                    break;
                }

                p_ipaddr = p_ipaddr->Next;
            }

NEXT:       
            pcuraddr = pcuraddr->Next;
        }
    }

    if (paddrs) 
    {
        free(paddrs);
    }

#elif defined(IOS)
    
    struct ifaddrs *if_addrs = NULL;
    struct ifaddrs *if_addr = NULL;
    struct sockaddr_in * sin;
    
    if (0 == getifaddrs(&if_addrs))
    {
        for (if_addr = if_addrs; if_addr != NULL; if_addr = if_addr->ifa_next)
        {
            if (NULL == if_addr->ifa_addr || if_addr->ifa_addr->sa_family != AF_INET)
            {
                continue;
            }
            
            if (if_addr->ifa_flags & IFF_LOOPBACK) // loopback interface
            {
                continue;
            }
            
            NetworkInterfaceList * p_net_inf = onvif_add_NetworkInterface();
            if (NULL == p_net_inf)
            {
                continue;
            }

            p_net_inf->NetworkInterface.Enabled = TRUE;
            p_net_inf->NetworkInterface.InfoFlag = 1;
            p_net_inf->NetworkInterface.Info.NameFlag = 1;
            p_net_inf->NetworkInterface.IPv4Flag = 1;
            p_net_inf->NetworkInterface.IPv4.Enabled = TRUE;

            sin = (struct sockaddr_in *)if_addr->ifa_addr;
            
            strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, inet_ntoa(sin->sin_addr));
            strncpy(p_net_inf->NetworkInterface.Info.Name, if_addr->ifa_name, sizeof(p_net_inf->NetworkInterface.Info.Name)-1);
            
            // Mask
            if (if_addr->ifa_netmask != NULL)
            {
                sin = (struct sockaddr_in *)if_addr->ifa_netmask;
                
                p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = get_prefix_len_by_mask(inet_ntoa(sin->sin_addr));
            }
            
            p_net_inf->NetworkInterface.Info.MTUFlag = 1;
            p_net_inf->NetworkInterface.Info.MTU = 1500;
            p_net_inf->NetworkInterface.IPv4.Config.DHCP = 1;
            
            sprintf(p_net_inf->NetworkInterface.Info.HwAddress, "%02X:%02X:%02X:%02X:%02X:%02X",
                rand()&0xff, rand()&0xff, rand()&0xff, rand()&0xff, rand()&0xff, rand()&0xff);
        }
        
        freeifaddrs(if_addrs);
        if_addrs = NULL;
    }
    
#elif __LINUX_OS__

    int i;    
	int socket_fd;
	struct ifreq *ifr;
	struct ifconf conf;
	struct ifreq ifs[8];
	int num;
	
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	conf.ifc_len = sizeof(ifs);
	conf.ifc_req = ifs;
	
	ioctl(socket_fd, SIOCGIFCONF, &conf);
	
	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;
    
	for (i=0; i<num; i++)
	{		
		struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);
        
		if (ifr->ifr_addr.sa_family != AF_INET)
		{
			ifr++;
			continue;
		}
		
		if (ioctl(socket_fd, SIOCGIFFLAGS, ifr) == 0 && (ifr->ifr_flags & IFF_LOOPBACK) == 0) // not loopback interface
		{
			NetworkInterfaceList * p_net_inf = onvif_add_NetworkInterface();
			if (NULL == p_net_inf)
			{
				break;
			}	

			p_net_inf->NetworkInterface.Enabled = TRUE;
			p_net_inf->NetworkInterface.InfoFlag = 1;			
			p_net_inf->NetworkInterface.Info.NameFlag = 1;
			p_net_inf->NetworkInterface.IPv4Flag = 1;
			p_net_inf->NetworkInterface.IPv4.Enabled = TRUE;

			strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, inet_ntoa(sin->sin_addr));
			strncpy(p_net_inf->NetworkInterface.Info.Name, ifr->ifr_name, sizeof(p_net_inf->NetworkInterface.Info.Name)-1); 

			// get netmask
			if (ioctl(socket_fd, SIOCGIFNETMASK, ifr) == 0)
			{
				sin = (struct sockaddr_in *)(&ifr->ifr_netmask);
				p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = get_prefix_len_by_mask(inet_ntoa(sin->sin_addr));
			}
			else
			{
				p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = 24;
			}

			// get mtu
			if (ioctl(socket_fd, SIOCGIFMTU, ifr) == 0)
			{
				p_net_inf->NetworkInterface.Info.MTUFlag = 1;
				p_net_inf->NetworkInterface.Info.MTU = ifr->ifr_mtu;
			}
            
			// get hwaddr
	        if (ioctl(socket_fd, SIOCGIFHWADDR, ifr) == 0) 
	        {
	        	snprintf(p_net_inf->NetworkInterface.Info.HwAddress, sizeof(p_net_inf->NetworkInterface.Info.HwAddress), "%02X:%02X:%02X:%02X:%02X:%02X", 
					(uint8)ifr->ifr_hwaddr.sa_data[0], (uint8)ifr->ifr_hwaddr.sa_data[1], (uint8)ifr->ifr_hwaddr.sa_data[2],
					(uint8)ifr->ifr_hwaddr.sa_data[3], (uint8)ifr->ifr_hwaddr.sa_data[4], (uint8)ifr->ifr_hwaddr.sa_data[5]);
	        }
		}
		
		ifr++;
	}

	closesocket(socket_fd);
	
#endif	
}

void onvif_init_net()
{
    const char * dns;
    const char * gw;

    g_onvif_cfg.network.DiscoveryMode = DiscoveryMode_Discoverable;
    
	// init host name
    g_onvif_cfg.network.HostnameInformation.FromDHCP = FALSE;
    g_onvif_cfg.network.HostnameInformation.RebootNeeded = FALSE;
    g_onvif_cfg.network.HostnameInformation.NameFlag = 1;
    gethostname(g_onvif_cfg.network.HostnameInformation.Name, sizeof(g_onvif_cfg.network.HostnameInformation.Name));

	// init dns setting
    g_onvif_cfg.network.DNSInformation.SearchDomainFlag = 1;
    g_onvif_cfg.network.DNSInformation.FromDHCP = FALSE;
    
    dns = get_dns_server();
    if (dns && strlen(dns) > 0)
    {
    	strcpy(g_onvif_cfg.network.DNSInformation.DNSServer[0], dns);
    }
    else
    {
    	strcpy(g_onvif_cfg.network.DNSInformation.DNSServer[0], "192.168.1.1");
	}

    // init ntp settting
    g_onvif_cfg.network.NTPInformation.FromDHCP = FALSE;
    strcpy(g_onvif_cfg.network.NTPInformation.NTPServer[0], "time.windows.com");

    // init network protocol
    g_onvif_cfg.network.NetworkProtocol.HTTPFlag = 1;
    g_onvif_cfg.network.NetworkProtocol.HTTPEnabled = 1;
#ifdef HTTPS    
    g_onvif_cfg.network.NetworkProtocol.HTTPSFlag = 1;
    g_onvif_cfg.network.NetworkProtocol.HTTPSEnabled = g_onvif_cfg.https_enable;
#else
	g_onvif_cfg.network.NetworkProtocol.HTTPSFlag = 0;
	g_onvif_cfg.network.NetworkProtocol.HTTPSEnabled = 0;
#endif
    g_onvif_cfg.network.NetworkProtocol.RTSPFlag = 1;
    g_onvif_cfg.network.NetworkProtocol.RTSPEnabled = 1;
    
    g_onvif_cfg.network.NetworkProtocol.HTTPPort[0] = 80;
    g_onvif_cfg.network.NetworkProtocol.HTTPSPort[0] = 443;
    g_onvif_cfg.network.NetworkProtocol.RTSPPort[0] = 554;

    // init default gateway
    gw = get_default_gateway();
    if (gw && strlen(gw) > 0)
    {
    	strcpy(g_onvif_cfg.network.NetworkGateway.IPv4Address[0], gw);
    }
    else
    {
    	strcpy(g_onvif_cfg.network.NetworkGateway.IPv4Address[0], "192.168.1.1");
	}
    
    // init network interface
    onvif_init_NetworkInterface();

    if (g_onvif_cfg.network.interfaces)
    {
        // init zero configuration
        strcpy(g_onvif_cfg.network.ZeroConfiguration.InterfaceToken, g_onvif_cfg.network.interfaces->NetworkInterface.token);
    }
}

HT_API void onvif_init_cfg()
{
    if (onvif_read_device_uuid(g_onvif_cfg.EndpointReference, sizeof(g_onvif_cfg.EndpointReference)-1) == FALSE)
    {
	    strncpy(g_onvif_cfg.EndpointReference, onvif_uuid_create(), sizeof(g_onvif_cfg.EndpointReference)-1);

	    onvif_save_device_uuid(g_onvif_cfg.EndpointReference);
	}

#ifdef PROFILE_Q_SUPPORT
	g_onvif_cfg.device_state = onvif_read_device_state();
#endif

	g_onvif_cfg.evt_sim_interval = 60;
	g_onvif_cfg.evt_sim_flag = 1;
	g_onvif_cfg.evt_renew_time = 60;
	g_onvif_cfg.http_max_users = 16;

	onvif_init_SystemDateTime();

	// onvif_init_VideoSource();
	onvif_init_VideoSourceConfigurationOptions();
	onvif_init_VideoEncoderConfigurationOptions();	

#ifdef MEDIA2_SUPPORT
    onvif_init_MaskOptions();
#endif

#ifdef AUDIO_SUPPORT
	onvif_init_AudioSource();
	onvif_init_AudioEncoderConfigurationOptions();
	onvif_init_AudioDecoderConfigurations();
#endif

	onvif_init_ImagingSettings();
	onvif_init_ImagingOptions();

	onvif_init_MetadataConfiguration();
	onvif_init_MetadataConfigurationOptions();

	onvif_init_OSDConfigurationOptions();

#ifdef VIDEO_ANALYTICS
	onvif_init_VideoAnalyticsConfiguration();
#endif

#ifdef PROFILE_C_SUPPORT    
    onvif_init_DoorList();
    onvif_init_AreaInfoList();
    onvif_init_AccessPointList();
#endif

#ifdef DEVICEIO_SUPPORT
#ifdef AUDIO_SUPPORT
    // If do not support rtsp back channel, you can comment it out
    onvif_init_AudioOutput();
#endif    
    onvif_init_RelayOutput();
    onvif_init_DigitInput();
    onvif_init_SerialPort();
#endif

	onvif_load_cfg();

#ifdef PROFILE_G_SUPPORT
    onvif_init_Recording();
    onvif_init_RecordingJob();

    g_onvif_cfg.replay_session_timeout = 60;
#endif

#ifdef ACCESS_RULES
    onvif_init_AccessProfile();
#endif

#ifdef CREDENTIAL_SUPPORT
    onvif_init_Credential();
#endif

#ifdef SCHEDULE_SUPPORT
    onvif_init_Schedule();
#endif
}

void onvif_chk_server_cfg()
{
	int i;

    for (i = 0; i < g_onvif_cfg.servs_num; i++)
    {
        // check the server ip and port configure
        
        if (g_onvif_cfg.servs[i].serv_port <= 0 || g_onvif_cfg.servs[i].serv_port >= 65535) 
    	{
#ifdef HTTPS
    		if (g_onvif_cfg.https_enable)
    		{
    			g_onvif_cfg.servs[i].serv_port = 443;
    		}
    		else
    		{
    			g_onvif_cfg.servs[i].serv_port = 8000;
    		}
#else
    		g_onvif_cfg.servs[i].serv_port = 8000;
#endif
    	}

    	if (g_onvif_cfg.servs[i].serv_ip[0] == '\0')
    	{
    		const char * ip = onvif_get_local_ip();
    		if (ip)
    		{
    			strcpy(g_onvif_cfg.servs[i].serv_ip, ip);
    		}
    	}
    }    
}

HT_API void onvif_init()
{
#ifdef PTZ_SUPPORT
    ONVIF_PROFILE * p_profile;
#endif

	memset(&g_onvif_cfg, 0, sizeof(ONVIF_CFG));
	memset(&g_onvif_cls, 0, sizeof(ONVIF_CLS));
	
	onvif_init_cfg();

    onvif_chk_server_cfg();
    
    onvif_init_net();    

    onvif_eua_init();
    
#ifdef PTZ_SUPPORT
	onvif_init_ptz();
	
	// add PTZ node to profile
	p_profile = g_onvif_cfg.profiles;
	while (p_profile)
	{
		p_profile->ptz_cfg = g_onvif_cfg.ptz_cfg;
		p_profile->ptz_cfg->Configuration.UseCount++;
		
		p_profile = p_profile->next;
	}
#endif

#ifdef MEDIA_SUPPORT
    onvif_add_scope("onvif://www.onvif.org/Profile/Streaming", TRUE);
#endif

#ifdef MEDIA2_SUPPORT
    onvif_add_scope("onvif://www.onvif.org/Profile/T", TRUE);
#endif

#ifdef PROFILE_G_SUPPORT
    onvif_add_scope("onvif://www.onvif.org/Profile/G", TRUE);
#endif

#ifdef PROFILE_C_SUPPORT
    onvif_add_scope("onvif://www.onvif.org/Profile/C", TRUE);
#endif

#ifdef ACCESS_RULES
    onvif_add_scope("onvif://www.onvif.org/Profile/A", TRUE);
#endif

	onvif_init_capabilities();
}




