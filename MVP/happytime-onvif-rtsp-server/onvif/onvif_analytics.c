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
#include "onvif_cm.h"
#include "onvif_analytics.h"

#ifdef VIDEO_ANALYTICS

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

/***************************************************************************************/

/**
 The possible return value
 	ONVIF_ERR_NoConfig
*/
ONVIF_RET onvif_tan_GetSupportedRules(tan_GetSupportedRules_REQ * p_req, tan_GetSupportedRules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;

	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	memcpy(&p_res->SupportedRules, &p_va_cfg->SupportedRules, sizeof(onvif_SupportedRules));
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
 	ONVIF_ERR_InvalidRule,
	ONVIF_ERR_RuleAlreadyExistent
	ONVIF_ERR_TooManyRules
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_CreateRules(tan_CreateRules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;

	if (NULL == p_req->Rule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_req->Rule->Config.Name);
	if (NULL != p_config)
	{
		return ONVIF_ERR_RuleAlreadyExistent;
	}
	
	// check rule configuration pararmeters ...

	p_config = p_va_cfg->Configuration.RuleEngineConfiguration.Rule;
	if (NULL == p_config)
	{
		p_va_cfg->Configuration.RuleEngineConfiguration.Rule = p_req->Rule;
	}
	else
	{
		while (p_config && p_config->next) p_config = p_config->next;

		p_config->next = p_req->Rule;
	}
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
	ONVIF_ERR_RuleNotExistent
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_DeleteRules(tan_DeleteRules_REQ * p_req)
{
	int i;
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	for (i = 0; i < p_req->sizeRuleName; i++)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_req->RuleName[i]);
		if (NULL == p_config)
		{
			return ONVIF_ERR_RuleNotExistent;
		}

		onvif_remove_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_config);
	}
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
*/
ONVIF_RET onvif_tan_GetRules(tan_GetRules_REQ * p_req, tan_GetRules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_res->Rule = p_va_cfg->Configuration.RuleEngineConfiguration.Rule;
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
 	ONVIF_ERR_InvalidRule,
	ONVIF_ERR_RuleNotExistent
	ONVIF_ERR_TooManyRules
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_ModifyRules(tan_ModifyRules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	ConfigList * p_tmp;
	ConfigList * p_prev;

	if (NULL == p_req->Rule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_tmp = p_req->Rule;
	while (p_tmp)
	{
		// check rule configuration parameters ...

		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_tmp->Config.Name);
		if (NULL == p_config)
		{
			onvif_free_Configs(&p_tmp);	// free resource
			
			return ONVIF_ERR_RuleNotExistent;
		}

		p_prev = onvif_get_prev_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, p_config);
		if (NULL == p_prev)
		{
			p_va_cfg->Configuration.RuleEngineConfiguration.Rule = p_tmp;
			p_tmp->next = p_config->next;
		}
		else
		{
			p_prev->next = p_tmp;
			p_tmp->next = p_config->next;
		}

		onvif_free_Config(p_config);
		free(p_config);

		p_tmp = p_tmp->next;
	}
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
 	ONVIF_ERR_NameAlreadyExistent
	ONVIF_ERR_TooManyModules
	ONVIF_ERR_InvalidModule
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_CreateAnalyticsModules(tan_CreateAnalyticsModules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;

	if (NULL == p_req->AnalyticsModule)
	{
		return ONVIF_ERR_InvalidModule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, 
	    p_req->AnalyticsModule->Config.Name);
	if (NULL != p_config)
	{
		return ONVIF_ERR_NameAlreadyExistent;
	}
	
	// check analytics module configuration pararmeters ...

	p_config = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
	if (NULL == p_config)
	{
		p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule = p_req->AnalyticsModule;
	}
	else
	{
		while (p_config && p_config->next) p_config = p_config->next;

		p_config->next = p_req->AnalyticsModule;
	}
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
 	ONVIF_ERR_InvalidModule
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_DeleteAnalyticsModules(tan_DeleteAnalyticsModules_REQ * p_req)
{
	int i;
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	for (i = 0; i < p_req->sizeAnalyticsModuleName; i++)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, 
		    p_req->AnalyticsModuleName[i]);
		if (NULL == p_config)
		{
			return ONVIF_ERR_InvalidModule;
		}

		onvif_remove_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_config);
	}
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
*/
ONVIF_RET onvif_tan_GetAnalyticsModules(tan_GetAnalyticsModules_REQ * p_req, tan_GetAnalyticsModules_RES * p_res)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_res->AnalyticsModule = p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
	
	return ONVIF_OK;
}

/**
 The possible return value
 	ONVIF_ERR_NoConfig
 	ONVIF_ERR_InvalidModule
	ONVIF_ERR_TooManyModules
	ONVIF_ERR_InvalidModule
	ONVIF_ERR_ConfigurationConflict
*/
ONVIF_RET onvif_tan_ModifyAnalyticsModules(tan_ModifyAnalyticsModules_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ConfigList * p_config;
	ConfigList * p_tmp;
	ConfigList * p_prev;

	if (NULL == p_req->AnalyticsModule)
	{
		return ONVIF_ERR_InvalidRule;
	}
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_tmp = p_req->AnalyticsModule;
	while (p_tmp)
	{
		// check rule configuration parameters ...

		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, 
		    p_tmp->Config.Name);
		if (NULL == p_config)
		{
			onvif_free_Configs(&p_tmp);	// free resource
			
			return ONVIF_ERR_InvalidModule;
		}

		p_prev = onvif_get_prev_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, p_config);
		if (NULL == p_prev)
		{
			p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule = p_tmp;
			p_tmp->next = p_config->next;
		}
		else
		{
			p_prev->next = p_tmp;
			p_tmp->next = p_config->next;
		}

		onvif_free_Config(p_config);
		free(p_config);

		p_tmp = p_tmp->next;
	}
	
	return ONVIF_OK;
}


#endif	// end of VIDEO_ANALYTICS


