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
#include "onvif_ptz.h"
#include "onvif_utils.h"

#ifdef MEDIA2_SUPPORT
#include "onvif_media2.h"
#endif

#ifdef PTZ_SUPPORT

/***************************************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

/***************************************************************************************/

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoStatus
 **/
ONVIF_RET onvif_ptz_GetStatus(ONVIF_PROFILE * p_profile, onvif_PTZStatus * p_ptz_status)
{
    if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
	// todo : add get ptz status code ...
	
	p_ptz_status->PositionFlag = 1;
	p_ptz_status->Position.PanTiltFlag = 1;
	p_ptz_status->Position.PanTilt.x = 0;
	p_ptz_status->Position.PanTilt.y = 0;
	p_ptz_status->Position.ZoomFlag = 1;
	p_ptz_status->Position.Zoom.x = 0;
	
	p_ptz_status->MoveStatusFlag = 1;
	p_ptz_status->MoveStatus.PanTiltFlag = 1;
	p_ptz_status->MoveStatus.PanTilt = MoveStatus_IDLE;
	p_ptz_status->MoveStatus.ZoomFlag = 1;
	p_ptz_status->MoveStatus.Zoom = MoveStatus_IDLE;

	p_ptz_status->ErrorFlag = 0;
    p_ptz_status->UtcTime = time(NULL);	
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidTranslation
 *  ONVIF_ERR_TimeoutNotSupported
 *  ONVIF_ERR_InvalidVelocity
 **/
ONVIF_RET onvif_ptz_ContinuousMove(ptz_ContinuousMove_REQ * p_req)
{
    PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
    else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
	    return ONVIF_ERR_NoPTZProfile;
	}
	
	if (p_req->Velocity.PanTiltFlag)
	{
		if (p_req->Velocity.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Min < -FPP || 
		 	p_req->Velocity.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidVelocity;
		}

		if (p_req->Velocity.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Min < -FPP || 
			p_req->Velocity.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidVelocity;
		}
	}
	
	if (p_req->Velocity.ZoomFlag && 
		(p_req->Velocity.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Min < -FPP || 
		 p_req->Velocity.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidVelocity;
	}
	
	// todo : add your handler code ...
	
	
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 **/
ONVIF_RET onvif_ptz_Stop(ptz_Stop_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	// todo : add your handler code ...

	
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidPosition
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_AbsoluteMove(ptz_AbsoluteMove_REQ * p_req)
{
    PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

    p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
	    return ONVIF_ERR_NoPTZProfile;
	}
	
	if (p_req->Position.PanTiltFlag)
	{
		if (p_req->Position.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min < -FPP || 
		 	p_req->Position.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidPosition;
		}

		if (p_req->Position.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min < -FPP || 
			p_req->Position.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidPosition;
		}
	}

	if (p_req->Position.ZoomFlag && 
		(p_req->Position.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Min < -FPP || 
		 p_req->Position.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidPosition;
	}	
	
	// todo : add your handler code ...

	
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_InvalidTranslation
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_RelativeMove(ptz_RelativeMove_REQ * p_req)
{
    PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

    p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node)
	{
	    return ONVIF_ERR_NoPTZProfile;
	}
	
	if (p_req->Translation.PanTiltFlag)
	{
		if (p_req->Translation.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Min < -FPP || 
			p_req->Translation.PanTilt.x - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidTranslation;
		}

		if (p_req->Translation.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Min < -FPP || 
			p_req->Translation.PanTilt.y - p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Max > FPP)
		{
			return ONVIF_ERR_InvalidTranslation;
		}
	}
	
	if (p_req->Translation.ZoomFlag && 
		(p_req->Translation.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Min < -FPP || 
		 p_req->Translation.Zoom.x - p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Max > FPP))
	{
		return ONVIF_ERR_InvalidTranslation;
	}
	
	// todo : add your handler code ...

	
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_PresetExist
 *  ONVIF_ERR_InvalidPresetName
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_MovingPTZ
 *  ONVIF_ERR_TooManyPresets
 **/
ONVIF_RET onvif_ptz_SetPreset(ptz_SetPreset_REQ * p_req)
{
    PTZPresetList * p_preset = NULL;
	ONVIF_PROFILE * p_profile;

	p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}
	
    if (p_req->PresetTokenFlag && p_req->PresetToken[0] != '\0')
    {
        p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
        if (NULL == p_preset)
        {
        	return ONVIF_ERR_NoToken;
        }
    }
    else
    {
        p_preset = onvif_add_PTZPreset(&p_profile->presets);
        if (NULL == p_preset)
        {
        	return ONVIF_ERR_TooManyPresets;
        }
    }

    if (p_req->PresetNameFlag && p_req->PresetName[0] != '\0')
    {
    	strcpy(p_preset->PTZPreset.Name, p_req->PresetName);
    }
    else
    {
    	sprintf(p_preset->PTZPreset.Name, "PRESET_%d", g_onvif_cls.preset_idx);
    	strcpy(p_req->PresetName, p_preset->PTZPreset.Name);
    	g_onvif_cls.preset_idx++;
    }
    
    if (p_req->PresetTokenFlag && p_req->PresetToken[0] != '\0')
    {
        strcpy(p_preset->PTZPreset.token, p_req->PresetToken);
    }
    else
    {
        sprintf(p_preset->PTZPreset.token, "PRESET_%d", g_onvif_cls.preset_idx);
        strcpy(p_req->PresetToken, p_preset->PTZPreset.token);
        g_onvif_cls.preset_idx++;
    }

    // todo : get PTZ current position ...
    p_preset->PTZPreset.PTZPositionFlag = 1;
    p_preset->PTZPreset.PTZPosition.PanTiltFlag = 1;
    p_preset->PTZPreset.PTZPosition.PanTilt.x = 0;
    p_preset->PTZPreset.PTZPosition.PanTilt.y = 0;
    p_preset->PTZPreset.PTZPosition.ZoomFlag = 1;
    p_preset->PTZPreset.PTZPosition.Zoom.x = 0;
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_RemovePreset(ptz_RemovePreset_REQ * p_req)
{
    ONVIF_PROFILE * p_profile;
    PTZPresetList * p_preset;

    p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

    p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
    if (NULL == p_preset)
    {
        return ONVIF_ERR_NoToken;
    }

    onvif_free_PTZPreset(&p_profile->presets, p_preset);

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_SpaceNotSupported
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_GotoPreset(ptz_GotoPreset_REQ * p_req)
{	
	ONVIF_PROFILE * p_profile;
    PTZPresetList * p_preset;
    
	p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
    else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

    p_preset = onvif_find_PTZPreset(p_profile->presets, p_req->PresetToken);
    if (NULL == p_preset)
    {
        return ONVIF_ERR_NoToken;
    }

    // todo : add your handler code ...
    

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoHomePosition
 *  ONVIF_ERR_InvalidSpeed
 **/
ONVIF_RET onvif_ptz_GotoHomePosition(ptz_GotoHomePosition_REQ * p_req)
{
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
    	return ONVIF_ERR_NoPTZProfile;
    }

    // todo : add your handler code ...

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_CannotOverwriteHome
 **/
ONVIF_RET onvif_ptz_SetHomePosition(const char * token)
{
    PTZNodeList * p_node;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
    	return ONVIF_ERR_NoPTZProfile;
    }

    p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
    if (NULL == p_node)
    {
        return ONVIF_ERR_NoPTZProfile;
    }
    
	if (p_node->PTZNode.FixedHomePosition)
	{
		return ONVIF_ERR_CannotOverwriteHome;
	}
	
    // todo : add your handler code ...

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_ConfigModify
 *  ONVIF_ERR_ConfigurationConflict
 **/
ONVIF_RET onvif_ptz_SetConfiguration(ptz_SetConfiguration_REQ * p_req)
{
	PTZConfigurationList * p_ptz_cfg;
	PTZNodeList * p_ptz_node;

	p_ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_req->PTZConfiguration.token);
	if (NULL == p_ptz_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_ptz_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_req->PTZConfiguration.NodeToken);
	if (NULL == p_ptz_node)
	{
		return ONVIF_ERR_ConfigModify;
	}

	if (p_req->PTZConfiguration.DefaultPTZTimeoutFlag)
	{
		if (p_req->PTZConfiguration.DefaultPTZTimeout < g_onvif_cfg.PTZConfigurationOptions.PTZTimeout.Min ||
			p_req->PTZConfiguration.DefaultPTZTimeout > g_onvif_cfg.PTZConfigurationOptions.PTZTimeout.Max)
		{
			return ONVIF_ERR_ConfigModify;
		}
	}

	// todo : add your handler code ...


    if (p_req->PTZConfiguration.MoveRampFlag)
    {
        p_ptz_cfg->Configuration.MoveRamp = p_req->PTZConfiguration.MoveRamp;
    }
    
    if (p_req->PTZConfiguration.PresetRampFlag)
    {
        p_ptz_cfg->Configuration.PresetRamp = p_req->PTZConfiguration.PresetRamp;
    }
    
    if (p_req->PTZConfiguration.PresetTourRampFlag)
    {
        p_ptz_cfg->Configuration.PresetTourRamp = p_req->PTZConfiguration.PresetTourRamp;
    }    

	strcpy(p_ptz_cfg->Configuration.Name, p_req->PTZConfiguration.Name);
	
	if (p_req->PTZConfiguration.DefaultPTZSpeedFlag)
	{
		if (p_req->PTZConfiguration.DefaultPTZSpeed.PanTiltFlag)
		{
			p_ptz_cfg->Configuration.DefaultPTZSpeed.PanTilt.x = p_req->PTZConfiguration.DefaultPTZSpeed.PanTilt.x;
			p_ptz_cfg->Configuration.DefaultPTZSpeed.PanTilt.y = p_req->PTZConfiguration.DefaultPTZSpeed.PanTilt.y;
		}

		if (p_req->PTZConfiguration.DefaultPTZSpeed.ZoomFlag)
		{
			p_ptz_cfg->Configuration.DefaultPTZSpeed.Zoom.x = p_req->PTZConfiguration.DefaultPTZSpeed.Zoom.x;
		}
	}

	if (p_req->PTZConfiguration.DefaultPTZTimeoutFlag)
	{
		p_ptz_cfg->Configuration.DefaultPTZTimeout = p_req->PTZConfiguration.DefaultPTZTimeout;
	}

	if (p_req->PTZConfiguration.PanTiltLimitsFlag)
	{
		memcpy(&p_ptz_cfg->Configuration.PanTiltLimits, &p_req->PTZConfiguration.PanTiltLimits, sizeof(onvif_PanTiltLimits));
	}

	if (p_req->PTZConfiguration.ZoomLimitsFlag)
	{
		memcpy(&p_ptz_cfg->Configuration.ZoomLimits, &p_req->PTZConfiguration.ZoomLimits, sizeof(onvif_ZoomLimits));
	}

	if (p_req->PTZConfiguration.ExtensionFlag)
	{
		if (p_req->PTZConfiguration.Extension.PTControlDirectionFlag)
		{
			if (p_req->PTZConfiguration.Extension.PTControlDirection.EFlipFlag)
			{
				p_ptz_cfg->Configuration.Extension.PTControlDirection.EFlip = p_req->PTZConfiguration.Extension.PTControlDirection.EFlip;
			}

			if (p_req->PTZConfiguration.Extension.PTControlDirection.ReverseFlag)
			{
				p_ptz_cfg->Configuration.Extension.PTControlDirection.Reverse = p_req->PTZConfiguration.Extension.PTControlDirection.Reverse;
			}
		}
	}
    
#ifdef MEDIA2_SUPPORT
    onvif_MediaConfigurationChangedNotify(p_req->PTZConfiguration.token, "PTZ");
#endif

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_GetPresetTourOptions(ptz_GetPresetTourOptions_REQ * p_req, ptz_GetPresetTourOptions_RES * p_res)
{
    int cnt = 0;
    PTZPresetList * p_preset;
    PresetTourList * p_tour;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    if (p_req->PresetTourTokenFlag)
    {
        p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_req->PresetTourToken);
        if (NULL == p_tour)
        {
            return ONVIF_ERR_NoToken;
        }
    }

    // todo : add your handler code ...
    

    p_res->Options.AutoStart = FALSE;
    p_res->Options.StartingCondition.RecurringTimeFlag = 1;
    p_res->Options.StartingCondition.RecurringTime.Min = 10;
    p_res->Options.StartingCondition.RecurringTime.Max = 100;

    p_res->Options.StartingCondition.RecurringDurationFlag = 1;
    p_res->Options.StartingCondition.RecurringDuration.Min = 10;
    p_res->Options.StartingCondition.RecurringDuration.Max = 100;

    p_res->Options.StartingCondition.PTZPresetTourDirection_Backward = 1;
    p_res->Options.StartingCondition.PTZPresetTourDirection_Forward = 1;

    p_res->Options.TourSpot.PresetDetail.HomeFlag = 1;
    p_res->Options.TourSpot.PresetDetail.Home = TRUE;

    p_preset = p_profile->presets;
    while (p_preset)
    {
        strcpy(p_res->Options.TourSpot.PresetDetail.PresetToken[cnt], p_preset->PTZPreset.token);

        cnt++;
        if (cnt >= ARRAY_SIZE(p_res->Options.TourSpot.PresetDetail.PresetToken))
        {
            break;
        }
        
        p_preset = p_preset->next;
    }

    p_res->Options.TourSpot.PresetDetail.sizePresetToken = cnt;
    
    p_res->Options.TourSpot.StayTime.Min = 0;
    p_res->Options.TourSpot.StayTime.Max = 100;
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_TooManyPresetTours
 **/
ONVIF_RET onvif_ptz_CreatePresetTour(ptz_CreatePresetTour_REQ * p_req, ptz_CreatePresetTour_RES * p_res)
{
    int cnt;
    PTZNodeList * p_node;
    PresetTourList * p_tour;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
    if (NULL == p_node)
    {
        return ONVIF_ERR_NoPTZProfile;
    }
    
    cnt = onvif_count_PresetTours(p_profile->preset_tour);
    
    if (p_node->PTZNode.ExtensionFlag && 
        p_node->PTZNode.Extension.SupportedPresetTourFlag && 
        cnt >= p_node->PTZNode.Extension.SupportedPresetTour.MaximumNumberOfPresetTours)
    {
        return ONVIF_ERR_TooManyPresetTours;
    }

    sprintf(p_res->PresetTourToken, "presettour_%d", g_onvif_cls.preset_tour_idx++);

    p_tour = onvif_add_PresetTour(&p_profile->preset_tour);
    if (p_tour)
    {
        strcpy(p_tour->PresetTour.token, p_res->PresetTourToken);
    }
    else
    {
        return ONVIF_ERR_TooManyPresetTours;
    }
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_InvalidPresetTour
 *  ONVIF_ERR_TooManyPresets
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_SpaceNotSupported
 **/
ONVIF_RET onvif_ptz_ModifyPresetTour(ptz_ModifyPresetTour_REQ * p_req)
{
    PresetTourList * p_tour;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_req->PresetTour.token);
    if (NULL == p_tour)
    {
        return ONVIF_ERR_NoToken;
    }

    // todo : add your handler code ...


    strcpy(p_tour->PresetTour.Name, p_req->PresetTour.Name);
    memcpy(&p_tour->PresetTour.StartingCondition, &p_req->PresetTour.StartingCondition, sizeof(onvif_PTZPresetTourStartingCondition));

    onvif_free_PTZPresetTourSpots(&p_tour->PresetTour.TourSpot);

    p_tour->PresetTour.TourSpot = p_req->PresetTour.TourSpot;
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_InvalidPresetTour
 *  ONVIF_ERR_NoToken
 *  ONVIF_ERR_ActivationFailed
 **/
ONVIF_RET onvif_ptz_OperatePresetTour(ptz_OperatePresetTour_REQ * p_req)
{
    PresetTourList * p_tour;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_req->PresetTourToken);
    if (NULL == p_tour)
    {
        return ONVIF_ERR_NoToken;
    }

    // todo : add your handler code ...
    

    if (PTZPresetTourOperation_Start == p_req->Operation)
    {
        p_tour->PresetTour.Status.State = PTZPresetTourState_Touring;
    }
    else if (PTZPresetTourOperation_Pause == p_req->Operation)
    {
        p_tour->PresetTour.Status.State = PTZPresetTourState_Paused;
    }
    else if (PTZPresetTourOperation_Stop == p_req->Operation)
    {
        p_tour->PresetTour.Status.State = PTZPresetTourState_Idle;
    }
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_NoToken
 **/
ONVIF_RET onvif_ptz_RemovePresetTour(ptz_RemovePresetTour_REQ * p_req)
{
    PresetTourList * p_tour;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    p_tour = onvif_find_PresetTour(p_profile->preset_tour, p_req->PresetTourToken);
    if (NULL == p_tour)
    {
        return ONVIF_ERR_NoToken;
    }

    // todo : add your handler code ...
    

    onvif_free_PresetTour(&p_profile->preset_tour, p_tour);
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 **/
ONVIF_RET onvif_ptz_SendAuxiliaryCommand(ptz_SendAuxiliaryCommand_REQ * p_req, ptz_SendAuxiliaryCommand_RES * p_res)
{
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
    if (NULL == p_profile)
    {
        return ONVIF_ERR_NoProfile;
    }
    else if (NULL == p_profile->ptz_cfg)
    {
        return ONVIF_ERR_NoPTZProfile;
    }

    // todo : add your handler code ...

    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoPTZProfile
 *  ONVIF_ERR_GeoMoveNotSupported
 *  ONVIF_ERR_UnreachablePosition
 *  ONVIF_ERR_TimeoutNotSupported
 *  ONVIF_ERR_GeoLocationUnknown
 **/
ONVIF_RET onvif_ptz_GeoMove(ptz_GeoMove_REQ * p_req)
{
    PTZNodeList * p_node;
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	else if (NULL == p_profile->ptz_cfg)
	{
		return ONVIF_ERR_NoPTZProfile;
	}

	p_node = onvif_find_PTZNode(g_onvif_cfg.ptz_node, p_profile->ptz_cfg->Configuration.NodeToken);
	if (NULL == p_node || !p_node->PTZNode.GeoMove)
	{
	    return ONVIF_ERR_GeoMoveNotSupported;
	}

	// todo : add your handler code ... 
	
	
    return ONVIF_OK;
}

#endif // PTZ_SUPPORT


