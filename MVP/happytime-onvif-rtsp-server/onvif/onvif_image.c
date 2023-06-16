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
#include "onvif_image.h"

#ifdef IMAGE_SUPPORT

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;


/***************************************************************************************/

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 *  ONVIF_ERR_SettingsInvalid
 */
ONVIF_RET onvif_img_SetImagingSettings(img_SetImagingSettings_REQ * p_req)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	/* valid param value */
	
    if (p_req->ImagingSettings.BacklightCompensationFlag && p_req->ImagingSettings.BacklightCompensation.LevelFlag && 
    	(p_req->ImagingSettings.BacklightCompensation.Level - g_onvif_cfg.ImagingOptions.BacklightCompensation.Level.Min < -FPP || 
		 p_req->ImagingSettings.BacklightCompensation.Level - g_onvif_cfg.ImagingOptions.BacklightCompensation.Level.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
    
	if (p_req->ImagingSettings.BrightnessFlag && 
		(p_req->ImagingSettings.Brightness - g_onvif_cfg.ImagingOptions.Brightness.Min < -FPP || 
		 p_req->ImagingSettings.Brightness - g_onvif_cfg.ImagingOptions.Brightness.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ColorSaturationFlag && 
		(p_req->ImagingSettings.ColorSaturation - g_onvif_cfg.ImagingOptions.ColorSaturation.Min < -FPP || 
		 p_req->ImagingSettings.ColorSaturation - g_onvif_cfg.ImagingOptions.ColorSaturation.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ContrastFlag && 
		(p_req->ImagingSettings.Contrast - g_onvif_cfg.ImagingOptions.Contrast.Min < -FPP || 
		 p_req->ImagingSettings.Contrast - g_onvif_cfg.ImagingOptions.Contrast.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinExposureTimeFlag && 
		(p_req->ImagingSettings.Exposure.MinExposureTime - g_onvif_cfg.ImagingOptions.Exposure.MinExposureTime.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinExposureTime - g_onvif_cfg.ImagingOptions.Exposure.MinExposureTime.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxExposureTimeFlag && 
		(p_req->ImagingSettings.Exposure.MaxExposureTime - g_onvif_cfg.ImagingOptions.Exposure.MaxExposureTime.Min < -FPP|| 
		 p_req->ImagingSettings.Exposure.MaxExposureTime - g_onvif_cfg.ImagingOptions.Exposure.MaxExposureTime.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinGainFlag && 
		(p_req->ImagingSettings.Exposure.MinGain - g_onvif_cfg.ImagingOptions.Exposure.MinGain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinGain - g_onvif_cfg.ImagingOptions.Exposure.MinGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxGainFlag && 
		(p_req->ImagingSettings.Exposure.MaxGain - g_onvif_cfg.ImagingOptions.Exposure.MaxGain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MaxGain - g_onvif_cfg.ImagingOptions.Exposure.MaxGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinIrisFlag && 
		(p_req->ImagingSettings.Exposure.MinIris - g_onvif_cfg.ImagingOptions.Exposure.MinIris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinIris - g_onvif_cfg.ImagingOptions.Exposure.MinIris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxIrisFlag && 
		(p_req->ImagingSettings.Exposure.MaxIris - g_onvif_cfg.ImagingOptions.Exposure.MaxIris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MaxIris - g_onvif_cfg.ImagingOptions.Exposure.MaxIris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.ExposureTimeFlag && 
		(p_req->ImagingSettings.Exposure.ExposureTime - g_onvif_cfg.ImagingOptions.Exposure.ExposureTime.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.ExposureTime - g_onvif_cfg.ImagingOptions.Exposure.ExposureTime.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.GainFlag && 
		(p_req->ImagingSettings.Exposure.Gain - g_onvif_cfg.ImagingOptions.Exposure.Gain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.Gain - g_onvif_cfg.ImagingOptions.Exposure.Gain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.IrisFlag && 
		(p_req->ImagingSettings.Exposure.Iris - g_onvif_cfg.ImagingOptions.Exposure.Iris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.Iris - g_onvif_cfg.ImagingOptions.Exposure.Iris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	
	if (p_req->ImagingSettings.SharpnessFlag && 
		(p_req->ImagingSettings.Sharpness - g_onvif_cfg.ImagingOptions.Sharpness.Min < -FPP || 
		 p_req->ImagingSettings.Sharpness - g_onvif_cfg.ImagingOptions.Sharpness.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WideDynamicRangeFlag && p_req->ImagingSettings.WideDynamicRange.LevelFlag && 
		(p_req->ImagingSettings.WideDynamicRange.Level - g_onvif_cfg.ImagingOptions.WideDynamicRange.Level.Min < -FPP || 
		 p_req->ImagingSettings.WideDynamicRange.Level - g_onvif_cfg.ImagingOptions.WideDynamicRange.Level.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag && p_req->ImagingSettings.WhiteBalance.CrGainFlag && 
		(p_req->ImagingSettings.WhiteBalance.CrGain - g_onvif_cfg.ImagingOptions.WhiteBalance.YrGain.Min < -FPP || 
		 p_req->ImagingSettings.WhiteBalance.CrGain - g_onvif_cfg.ImagingOptions.WhiteBalance.YrGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag && p_req->ImagingSettings.WhiteBalance.CbGainFlag && 
		(p_req->ImagingSettings.WhiteBalance.CbGain - g_onvif_cfg.ImagingOptions.WhiteBalance.YbGain.Min < -FPP || 
		 p_req->ImagingSettings.WhiteBalance.CbGain - g_onvif_cfg.ImagingOptions.WhiteBalance.YbGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	
	// todo : add the image setting code ...
	


	// save image setting

	if (p_req->ImagingSettings.BacklightCompensationFlag)
	{
		g_onvif_cfg.ImagingSettings.BacklightCompensation.Mode = p_req->ImagingSettings.BacklightCompensation.Mode;
		
		if (p_req->ImagingSettings.BacklightCompensation.LevelFlag)
		{
			g_onvif_cfg.ImagingSettings.BacklightCompensation.Level = p_req->ImagingSettings.BacklightCompensation.Level;
		}
	}

	if (p_req->ImagingSettings.BrightnessFlag)
	{
		g_onvif_cfg.ImagingSettings.Brightness = p_req->ImagingSettings.Brightness;
	}

	if (p_req->ImagingSettings.ColorSaturationFlag)
	{
		g_onvif_cfg.ImagingSettings.ColorSaturation = p_req->ImagingSettings.ColorSaturation;
	}

	if (p_req->ImagingSettings.ContrastFlag)
	{
		g_onvif_cfg.ImagingSettings.Contrast = p_req->ImagingSettings.Contrast;
	}

	if (p_req->ImagingSettings.ExposureFlag)
	{
		g_onvif_cfg.ImagingSettings.Exposure.Mode = p_req->ImagingSettings.Exposure.Mode;

		if (p_req->ImagingSettings.Exposure.PriorityFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.Priority = p_req->ImagingSettings.Exposure.Priority;
		}

		if (p_req->ImagingSettings.Exposure.MinExposureTimeFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MinExposureTime = p_req->ImagingSettings.Exposure.MinExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.MaxExposureTimeFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MaxExposureTime = p_req->ImagingSettings.Exposure.MaxExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.MinGainFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MinGain = p_req->ImagingSettings.Exposure.MinGain;
		}

		if (p_req->ImagingSettings.Exposure.MaxGainFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MaxGain = p_req->ImagingSettings.Exposure.MaxGain;
		}

		if (p_req->ImagingSettings.Exposure.MinIrisFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MinIris = p_req->ImagingSettings.Exposure.MinIris;
		}

		if (p_req->ImagingSettings.Exposure.MaxIrisFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.MaxIris = p_req->ImagingSettings.Exposure.MaxIris;
		}

		if (p_req->ImagingSettings.Exposure.ExposureTimeFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.ExposureTime = p_req->ImagingSettings.Exposure.ExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.GainFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.Gain = p_req->ImagingSettings.Exposure.Gain;
		}

		if (p_req->ImagingSettings.Exposure.IrisFlag)
		{
			g_onvif_cfg.ImagingSettings.Exposure.Iris = p_req->ImagingSettings.Exposure.Iris;
		}
	}

	if (p_req->ImagingSettings.FocusFlag)
	{
		g_onvif_cfg.ImagingSettings.Focus.AutoFocusMode = p_req->ImagingSettings.Focus.AutoFocusMode;

		if (p_req->ImagingSettings.Focus.DefaultSpeedFlag)
		{
			g_onvif_cfg.ImagingSettings.Focus.DefaultSpeed = p_req->ImagingSettings.Focus.DefaultSpeed;
		}

		if (p_req->ImagingSettings.Focus.NearLimitFlag)
		{
			g_onvif_cfg.ImagingSettings.Focus.NearLimit = p_req->ImagingSettings.Focus.NearLimit;
		}

		if (p_req->ImagingSettings.Focus.FarLimitFlag)
		{
			g_onvif_cfg.ImagingSettings.Focus.FarLimit = p_req->ImagingSettings.Focus.FarLimit;
		}
	}

	if (p_req->ImagingSettings.IrCutFilterFlag)
	{
		g_onvif_cfg.ImagingSettings.IrCutFilter = p_req->ImagingSettings.IrCutFilter;
	}

	if (p_req->ImagingSettings.SharpnessFlag)
	{
		g_onvif_cfg.ImagingSettings.Sharpness = p_req->ImagingSettings.Sharpness;
	}

	if (p_req->ImagingSettings.WideDynamicRangeFlag)
	{
		g_onvif_cfg.ImagingSettings.WideDynamicRange.Mode = p_req->ImagingSettings.WideDynamicRange.Mode;

		if (p_req->ImagingSettings.WideDynamicRange.LevelFlag)
		{
			g_onvif_cfg.ImagingSettings.WideDynamicRange.Level = p_req->ImagingSettings.WideDynamicRange.Level;
		}
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag)
	{
		g_onvif_cfg.ImagingSettings.WhiteBalance.Mode = p_req->ImagingSettings.WhiteBalance.Mode;

		if (p_req->ImagingSettings.WhiteBalance.CrGainFlag)
		{
			g_onvif_cfg.ImagingSettings.WhiteBalance.CrGain = p_req->ImagingSettings.WhiteBalance.CrGain;
		}

		if (p_req->ImagingSettings.WhiteBalance.CbGainFlag)
		{
			g_onvif_cfg.ImagingSettings.WhiteBalance.CbGain = p_req->ImagingSettings.WhiteBalance.CbGain;
		}
	}
	
	return ONVIF_OK;
}

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_Move(img_Move_REQ * p_req)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

    if (p_req->Focus.AbsoluteFlag || p_req->Focus.RelativeFlag)
    {
        return ONVIF_ERR_NotSupported;
    }

    if (p_req->Focus.ContinuousFlag)
    {
        // check the parameter range 
        if (p_req->Focus.Continuous.Speed < 1.0f ||
            p_req->Focus.Continuous.Speed > 5.0f)
        {
            return ONVIF_ERR_SettingsInvalid;
        }
    }
	
	// todo : add move code ...
    
	return ONVIF_OK;
}

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_Stop(img_Stop_REQ * p_req)
{
    VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

    // todo : add stop move code ...
    
	return ONVIF_OK;
}

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_GetStatus(img_GetStatus_REQ * p_req, img_GetStatus_RES * p_res)
{
    VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
    if (NULL == p_v_src)
    {
        return ONVIF_ERR_NoVideoSource;
    }
    
	// todo : add get imaging status code ...

	p_res->Status.FocusStatusFlag = 1;
	p_res->Status.FocusStatus.Position = 0.0;
	p_res->Status.FocusStatus.MoveStatus = MoveStatus_IDLE;

	return ONVIF_OK;
}

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_GetMoveOptions(img_GetMoveOptions_REQ * p_req, img_GetMoveOptions_RES * p_res)
{
    VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
    if (NULL == p_v_src)
    {
        return ONVIF_ERR_NoVideoSource;
    }
    
    // todo : add get imaging move options code ...

    p_res->MoveOptions.ContinuousFlag = 1;
    p_res->MoveOptions.Continuous.Speed.Min = 1.0f;
    p_res->MoveOptions.Continuous.Speed.Max = 5.0f;

    return ONVIF_OK;
}

/**
 * possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 */
ONVIF_RET onvif_img_SetCurrentPreset(img_SetCurrentPreset_REQ * p_req)
{
    VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
    if (NULL == p_v_src)
    {
        return ONVIF_ERR_NoVideoSource;
    }

    strcpy(p_v_src->CurrentPrestToken, p_req->PresetToken);
    
    // todo : add your handler code ...

    return ONVIF_OK;
}

#endif // IMAGE_SUPPORT


