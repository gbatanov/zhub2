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

#include "onvif_device.h"
#include "sys_inc.h"
#include "onvif.h"
#include "xml_node.h"
#include "onvif_utils.h"
#include "onvif_cfg.h"
#include "onvif_probe.h"
#include "onvif_event.h"

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

/***************************************************************************************/

void onvif_LastClockSynchronizationNotify()
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Monitoring/OperatingTime/LastClockSynchronization", PropertyOperation_Changed, 
        NULL, NULL, NULL, NULL, 
        "Status", onvif_format_datetime_str(time(NULL), 1, "%Y-%m-%dT%H:%M:%SZ"), NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}


#ifdef PROFILE_Q_SUPPORT

void onvif_switchDeviceState(int state)
{
    // 0 - Factory Default state, 1 - Operational State

    if (state != g_onvif_cfg.device_state)
    {
        g_onvif_cfg.device_state = state;

	    onvif_save_device_state(state);
    }
    
    // When switching from Factory Default State to Operational State, the device may reboot if necessary.
    
}

#endif

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_AccesslogUnavailable
 *  ONVIF_ERR_SystemlogUnavailable
 **/
ONVIF_RET onvif_tds_GetSystemLog(tds_GetSystemLog_REQ * p_req, tds_GetSystemLog_RES * p_res)
{
    if (SystemLogType_Access == p_req->LogType)
    {
        strcpy(p_res->String, "test access log");
    }
    else
    {
        strcpy(p_res->String, "test system log");
    }
    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidTimeZone
 *  ONVIF_ERR_InvalidDateTime
 *  ONVIF_ERR_NtpServerUndefined
 **/
ONVIF_RET onvif_tds_SetSystemDateAndTime(tds_SetSystemDateAndTime_REQ * p_req)
{
	// check datetime
	if (p_req->SystemDateTime.DateTimeType == SetDateTimeType_Manual)
	{
		if (p_req->UTCDateTime.Date.Month < 1 || p_req->UTCDateTime.Date.Month > 12 ||
			p_req->UTCDateTime.Date.Day < 1 || p_req->UTCDateTime.Date.Day > 31 ||
			p_req->UTCDateTime.Time.Hour < 0 || p_req->UTCDateTime.Time.Hour > 23 ||
			p_req->UTCDateTime.Time.Minute < 0 || p_req->UTCDateTime.Time.Minute > 59 ||
			p_req->UTCDateTime.Time.Second < 0 || p_req->UTCDateTime.Time.Second > 61)
		{
			return ONVIF_ERR_InvalidDateTime;
		}
	}

	// check timezone
	if (p_req->SystemDateTime.TimeZoneFlag && 
		p_req->SystemDateTime.TimeZone.TZ[0] != '\0' && 
		onvif_is_valid_timezone(p_req->SystemDateTime.TimeZone.TZ) == FALSE)
	{
		return ONVIF_ERR_InvalidTimeZone;
	}

	// todo : add your handler code ...
	

	g_onvif_cfg.SystemDateTime.DateTimeType = p_req->SystemDateTime.DateTimeType;
	g_onvif_cfg.SystemDateTime.DaylightSavings = p_req->SystemDateTime.DaylightSavings;
	if (p_req->SystemDateTime.TimeZoneFlag && p_req->SystemDateTime.TimeZone.TZ[0] != '\0')
	{
		strcpy(g_onvif_cfg.SystemDateTime.TimeZone.TZ, p_req->SystemDateTime.TimeZone.TZ);
	}

    // send notify message ...
    onvif_LastClockSynchronizationNotify();
    
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SystemReboot()
{
	// todo : add your handler code ...

    // send onvif bye message    
    sleep(3);
    onvif_bye();

    // please comment the code below
    // send onvif hello message, just for test
    sleep(3);
    onvif_hello();
    
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetSystemFactoryDefault(tds_SetSystemFactoryDefault_REQ * p_req)
{
    // todo : add your handler code ...
    

#ifdef PROFILE_Q_SUPPORT
    onvif_switchDeviceState(0); // Devices conformant to Profile Q shall be in Factory Default State 
                                // out-of-the-box and after hard factory reset
#endif

    // todo : please note the following lines, just for test
    // send onvif hello message
    sleep(3);
    onvif_hello();

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidHostname
 **/
ONVIF_RET onvif_tds_SetHostname(tds_SetHostname_REQ * p_req)
{
    if (p_req->Name[0] == '\0')
    {
        return ONVIF_ERR_InvalidHostname;
    }
    else if (onvif_is_valid_hostname(p_req->Name) == FALSE)
    {
    	return ONVIF_ERR_InvalidHostname;
    }
    
    // todo : add your handler code ...
    

    strncpy(g_onvif_cfg.network.HostnameInformation.Name, p_req->Name, sizeof(g_onvif_cfg.network.HostnameInformation.Name)-1);

    g_onvif_cfg.network.HostnameInformation.FromDHCP = FALSE;

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetHostnameFromDHCP(tds_SetHostnameFromDHCP_REQ * p_req)
{
    // todo : add your handler code ...
    

    g_onvif_cfg.network.HostnameInformation.FromDHCP = p_req->FromDHCP;

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetDNS(tds_SetDNS_REQ * p_req)
{
	// todo : add your handler code ...

	g_onvif_cfg.network.DNSInformation.FromDHCP = p_req->DNSInformation.FromDHCP;

	if (g_onvif_cfg.network.DNSInformation.FromDHCP == FALSE)
	{
		if (p_req->DNSInformation.SearchDomainFlag)
		{
			memcpy(g_onvif_cfg.network.DNSInformation.SearchDomain, 
			    p_req->DNSInformation.SearchDomain, 
			    sizeof(g_onvif_cfg.network.DNSInformation.SearchDomain));
		}

		memcpy(g_onvif_cfg.network.DNSInformation.DNSServer, 
		    p_req->DNSInformation.DNSServer, 
		    sizeof(g_onvif_cfg.network.DNSInformation.DNSServer));
    }
	else
	{
		// todo : get dns server from dhcp ...
		
	}
    
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetDynamicDNS(tds_SetDynamicDNS_REQ * p_req)
{
    // todo : add your handler code ...

    memcpy(&g_onvif_cfg.network.DynamicDNSInformation, &p_req->DynamicDNSInformation, sizeof(onvif_DynamicDNSInformation));

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_InvalidDnsName
 **/
ONVIF_RET onvif_tds_SetNTP(tds_SetNTP_REQ * p_req)
{
	// todo : add your handler code ...

	g_onvif_cfg.network.NTPInformation.FromDHCP = p_req->NTPInformation.FromDHCP;

	if (g_onvif_cfg.network.NTPInformation.FromDHCP == FALSE)
	{
		memcpy(g_onvif_cfg.network.NTPInformation.NTPServer, 
		    p_req->NTPInformation.NTPServer, 
		    sizeof(g_onvif_cfg.network.NTPInformation.NTPServer));
    }
    else
    {
    	// todo : get ntp server from dhcp ...
    	
    }
    
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 **/
ONVIF_RET onvif_tds_SetZeroConfiguration(tds_SetZeroConfiguration_REQ * p_req)
{
    if (strcmp(p_req->InterfaceToken, g_onvif_cfg.network.ZeroConfiguration.InterfaceToken))
    {
        return ONVIF_ERR_InvalidNetworkInterface;
    }

    // todo : add your handler code ...

    g_onvif_cfg.network.ZeroConfiguration.Enabled = p_req->Enabled;

    // please comment the code below, just for test
    if (p_req->Enabled)
    {
        g_onvif_cfg.network.ZeroConfiguration.sizeAddresses = 1;
        strcpy(g_onvif_cfg.network.ZeroConfiguration.Addresses[0], "192.168.1.100");
    }
    else
    {
        g_onvif_cfg.network.ZeroConfiguration.sizeAddresses = 0;
    }

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_ServiceNotSupported
 *  ONVIF_ERR_PortAlreadyInUse
 **/
ONVIF_RET onvif_tds_SetNetworkProtocols(tds_SetNetworkProtocols_REQ * p_req)
{
#ifndef HTTPS
    if (p_req->NetworkProtocol.HTTPSFlag && p_req->NetworkProtocol.HTTPSEnabled)
    {
        return ONVIF_ERR_ServiceNotSupported;
    }
#endif

	// todo : add your handler code ...


	if (p_req->NetworkProtocol.HTTPFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.HTTPEnabled = p_req->NetworkProtocol.HTTPEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.HTTPPort, 
		    p_req->NetworkProtocol.HTTPPort, 
		    sizeof(g_onvif_cfg.network.NetworkProtocol.HTTPPort));
	}

	if (p_req->NetworkProtocol.HTTPSFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.HTTPSEnabled = p_req->NetworkProtocol.HTTPSEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.HTTPSPort, 
		    p_req->NetworkProtocol.HTTPSPort, 
		    sizeof(g_onvif_cfg.network.NetworkProtocol.HTTPSPort));
	}

	if (p_req->NetworkProtocol.RTSPFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.RTSPEnabled = p_req->NetworkProtocol.RTSPEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.RTSPPort, 
		    p_req->NetworkProtocol.RTSPPort, 
		    sizeof(g_onvif_cfg.network.NetworkProtocol.RTSPPort));
	}
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidGatewayAddress
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetNetworkDefaultGateway(tds_SetNetworkDefaultGateway_REQ * p_req)
{
	// todo : add your handler code ...

	memcpy(g_onvif_cfg.network.NetworkGateway.IPv4Address, 
	    p_req->IPv4Address, 
	    sizeof(g_onvif_cfg.network.NetworkGateway.IPv4Address));

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_GetSystemUris(const char * lip, uint32 lport, tds_GetSystemUris_RES * p_res)
{
    p_res->AccessLogUriFlag = 1;
    p_res->SystemLogUriFlag = 1;
    p_res->SupportInfoUriFlag = 1;
    p_res->SystemBackupUriFlag = 1;
    
    sprintf(p_res->SystemLogUri, "%s://%s:%d/SystemLog", g_onvif_cfg.https_enable ? "https" : "http", lip, lport);
    sprintf(p_res->AccessLogUri, "%s://%s:%d/AccessLog", g_onvif_cfg.https_enable ? "https" : "http", lip, lport);
    sprintf(p_res->SupportInfoUri, "%s://%s:%d/SupportInfo", g_onvif_cfg.https_enable ? "https" : "http", lip, lport);
    sprintf(p_res->SystemBackupUri, "%s://%s:%d/SystemBackup", g_onvif_cfg.https_enable ? "https" : "http", lip, lport);

    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidMtuValue
 *  ONVIF_ERR_InvalidInterfaceSpeed
 *  ONVIF_ERR_InvalidInterfaceType
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetNetworkInterfaces(tds_SetNetworkInterfaces_REQ * p_req)
{
    NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->NetworkInterface.token);
    if (NULL == p_net_inf)
    {
        return ONVIF_ERR_InvalidNetworkInterface;
    }

    if (p_req->NetworkInterface.InfoFlag && 
    	p_req->NetworkInterface.Info.MTUFlag && 
    	(p_req->NetworkInterface.Info.MTU < 0 || p_req->NetworkInterface.Info.MTU > 1530))
    {
        return ONVIF_ERR_InvalidMtuValue;
    }

    if (p_req->NetworkInterface.Enabled && 
        p_req->NetworkInterface.IPv4Flag && 
        p_req->NetworkInterface.IPv4.Enabled && 
        p_req->NetworkInterface.IPv4.Config.DHCP == FALSE)
    {
        if (is_ip_address(p_req->NetworkInterface.IPv4.Config.Address) == FALSE)
        {
            return ONVIF_ERR_InvalidIPv4Address;
        }
    }

    // todo : add your handler code ...
    

	p_net_inf->NetworkInterface.Enabled = p_req->NetworkInterface.Enabled;

	if (p_req->NetworkInterface.InfoFlag && p_req->NetworkInterface.Info.MTUFlag)
	{
		p_net_inf->NetworkInterface.Info.MTU = p_req->NetworkInterface.Info.MTU;
	}
    	
	if (p_req->NetworkInterface.IPv4Flag)
	{
		p_net_inf->NetworkInterface.IPv4.Enabled = p_req->NetworkInterface.IPv4.Enabled;
		p_net_inf->NetworkInterface.IPv4.Config.DHCP = p_req->NetworkInterface.IPv4.Config.DHCP;
		
		if (p_net_inf->NetworkInterface.IPv4.Config.DHCP == FALSE)
		{
	    	strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, p_req->NetworkInterface.IPv4.Config.Address);
	    	p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = p_req->NetworkInterface.IPv4.Config.PrefixLength;
	    }
    }

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetDiscoveryMode(tds_SetDiscoveryMode_REQ * p_req)
{
	g_onvif_cfg.network.DiscoveryMode = p_req->DiscoveryMode;

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidDot11
 *  ONVIF_ERR_NotDot11
 *  ONVIF_ERR_NotConnectedDot11
 **/
ONVIF_RET onvif_tds_GetDot11Status(tds_GetDot11Status_REQ * p_req, tds_GetDot11Status_RES * p_res)
{    
    NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->InterfaceToken);
    if (NULL == p_net_inf)
    {
        return ONVIF_ERR_InvalidNetworkInterface;
    }
    
    // todo : add your handler code ...

    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidDot11
 *  ONVIF_ERR_NotDot11
 *  ONVIF_ERR_NotScanAvailable
 **/
ONVIF_RET onvif_tds_ScanAvailableDot11Networks(tds_ScanAvailableDot11Networks_REQ * p_req, tds_ScanAvailableDot11Networks_RES * p_res)
{
    NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->InterfaceToken);
    if (NULL == p_net_inf)
    {
        return ONVIF_ERR_InvalidNetworkInterface;
    }

    if (g_onvif_cfg.Capabilities.dot11.ScanAvailableNetworks == 0)
    {
        return ONVIF_ERR_NotScanAvailable;
    }

    // todo : add your handler code ...

    
    return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameClash
 *  ONVIF_ERR_PasswordTooLong
 *  ONVIF_ERR_UsernameTooLong
 *  ONVIF_ERR_Password
 *  ONVIF_ERR_TooManyUsers
 *  ONVIF_ERR_AnonymousNotAllowed
 *  ONVIF_ERR_UsernameTooShort
 **/
ONVIF_RET onvif_tds_CreateUsers(tds_CreateUsers_REQ * p_req)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
	    int len;
	    onvif_User * p_idle_user;
	    
		if (p_req->User[i].Username[0] == '\0')
		{
			break;
		}

		len = (int)strlen(p_req->User[i].Username);
		if (len <= 3)
		{
			return ONVIF_ERR_UsernameTooShort;
		}

		if (onvif_is_user_exist(p_req->User[i].Username))
		{
			return ONVIF_ERR_UsernameClash;
		}
		
		p_idle_user = onvif_get_idle_user();
		if (p_idle_user)
		{
			memcpy(p_idle_user, &p_req->User[i], sizeof(onvif_User));

#ifdef PROFILE_Q_SUPPORT
            if (UserLevel_Administrator == p_req->User[i].UserLevel && 
                p_req->User[i].Password[0] != '\0')
            {
                onvif_switchDeviceState(1); // creates a new admin user, switch to Operational State
            }
#endif			
		}
		else
		{
			return ONVIF_ERR_TooManyUsers;
		}
	}
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameMissing
 *  ONVIF_ERR_FixedUser
 **/
ONVIF_RET onvif_tds_DeleteUsers(tds_DeleteUsers_REQ * p_req)
{
    int i;
	onvif_User * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->Username); i++)
	{
		if (p_req->Username[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_user(p_req->Username[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_UsernameMissing;
		}
		else if (p_item->fixed)
		{
			return ONVIF_ERR_FixedUser;
		}
	}	

	for (i = 0; i < ARRAY_SIZE(p_req->Username); i++)
	{
		if (p_req->Username[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_user(p_req->Username[i]);
		if (NULL != p_item && p_item->fixed == FALSE)
		{
			memset(p_item, 0, sizeof(onvif_User));
		}
	}

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameMissing
 *  ONVIF_ERR_FixedUser
 *  ONVIF_ERR_PasswordTooLong
 *  ONVIF_ERR_Password
 *  ONVIF_ERR_AnonymousNotAllowed
 **/
ONVIF_RET onvif_tds_SetUser(tds_SetUser_REQ * p_req)
{
    int i;
	onvif_User * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
		if (p_req->User[i].Username[0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_user(p_req->User[i].Username);
		if (NULL == p_item)
		{
			return ONVIF_ERR_UsernameMissing;
		}
		else if (p_item && p_item->fixed)
		{
			return ONVIF_ERR_FixedUser;
		}		
	}
	
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
		if (p_req->User[i].Username[0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_user(p_req->User[i].Username);
		if (p_item && FALSE == p_item->fixed)
		{
			strcpy(p_item->Password, p_req->User[i].Password);
			p_item->UserLevel = p_req->User[i].UserLevel;

#ifdef PROFILE_Q_SUPPORT
            if (UserLevel_Administrator == p_item->UserLevel && 
                p_item->Password[0] != '\0')
            {
                onvif_switchDeviceState(1); // modifies the password of an existing admin user, switch to Operational State
            }
#endif
		}
	}

	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NotRemoteUser
 **/
ONVIF_RET onvif_tds_GetRemoteUser(tds_GetRemoteUser_RES * p_res)
{
    // todo : add your handler code ...


    // return ONVIF_ERR_NotRemoteUser;

    if (g_onvif_cfg.RemoteUser.Username[0] == '\0')
    {
        p_res->RemoteUserFlag = 0;
    }
    else
    {
        p_res->RemoteUserFlag = 1;
        strcpy(p_res->RemoteUser.Username, g_onvif_cfg.RemoteUser.Username);
    }
    
    p_res->RemoteUser.UseDerivedPassword = g_onvif_cfg.RemoteUser.UseDerivedPassword;

    return ONVIF_OK; 
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NotRemoteUser
 **/
ONVIF_RET onvif_tds_SetRemoteUser(tds_SetRemoteUser_REQ * p_req)
{
    // todo : add your handler code ...

    // To remove the remote user SetRemoteUser should be called without the RemoteUser parameter

    // return ONVIF_ERR_NotRemoteUser;
    
    if (p_req->RemoteUserFlag)
    {
        g_onvif_cfg.RemoteUser.UseDerivedPassword = p_req->RemoteUser.UseDerivedPassword;
        strcpy(g_onvif_cfg.RemoteUser.Username, p_req->RemoteUser.Username);

        if (p_req->RemoteUser.PasswordFlag)
        {
            strcpy(g_onvif_cfg.RemoteUser.Password, p_req->RemoteUser.Password);
        }
    }
    else
    {
        g_onvif_cfg.RemoteUser.UseDerivedPassword = FALSE;
        strcpy(g_onvif_cfg.RemoteUser.Username, "");
        strcpy(g_onvif_cfg.RemoteUser.Password, "");
    }
    
    return ONVIF_OK; 
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_TooManyScopes
 **/
ONVIF_RET onvif_tds_AddScopes(tds_AddScopes_REQ * p_req)
{
    int i;
	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
	    onvif_Scope * p_item;
	    
		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		if (onvif_is_scope_exist(p_req->ScopeItem[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_scope();
		if (p_item)
		{
			p_item->ScopeDef = ScopeDefinition_Configurable;
			strcpy(p_item->ScopeItem, p_req->ScopeItem[i]);
		}
		else
		{
			return ONVIF_ERR_TooManyScopes;
		}
	}
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_TooManyScopes
 *  ONVIF_ERR_ScopeOverwrite
 **/
ONVIF_RET onvif_tds_SetScopes(tds_SetScopes_REQ * p_req)
{
    int i;
	onvif_Scope * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
	{
		if (p_req->Scopes[i][0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_scope(p_req->Scopes[i]);
		if (p_item && ScopeDefinition_Fixed == p_item->ScopeDef)
		{
			return ONVIF_ERR_ScopeOverwrite;
		}
	}
	
	for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
	{
		if (p_req->Scopes[i][0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_scope(p_req->Scopes[i]);
		if (p_item && ScopeDefinition_Configurable == p_item->ScopeDef)
		{
			strcpy(p_item->ScopeItem, p_req->Scopes[i]);
		}
		else
		{
		    p_item = onvif_get_idle_scope();
    		if (p_item)
    		{
    			p_item->ScopeDef = ScopeDefinition_Configurable;
    			strcpy(p_item->ScopeItem, p_req->Scopes[i]);
    		}
		}
	}
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_FixedScope
 *  ONVIF_ERR_NoScope
 **/
ONVIF_RET onvif_tds_RemoveScopes(tds_RemoveScopes_REQ * p_req)
{
    int i;
	onvif_Scope * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_req->ScopeItem[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoScope;
		}
		else if (ScopeDefinition_Fixed == p_item->ScopeDef)
		{
			return ONVIF_ERR_FixedScope;
		}
	}	

	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_req->ScopeItem[i]);
		if (NULL != p_item && ScopeDefinition_Configurable == p_item->ScopeDef)
		{
			memset(p_item, 0, sizeof(onvif_Scope));
		}
	}	

	return ONVIF_OK;
}

BOOL onvif_tds_StartFirmwareUpgrade(const char * lip, uint32 lport, tds_StartFirmwareUpgrade_RES * p_res)
{
	// todo : do some file upload prepare ...
	
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		sprintf(p_res->UploadUri, "https://%s:%d/FirmwareUpgrade", lip, lport);
	}
	else
	{
		sprintf(p_res->UploadUri, "http://%s:%d/FirmwareUpgrade", lip, lport);
	}
#else
	sprintf(p_res->UploadUri, "http://%s:%d/FirmwareUpgrade", lip, lport);
#endif

	p_res->UploadDelay = 5;				// 5 seconds
	p_res->ExpectedDownTime = 5 * 60; 	// 5 minutes

	return TRUE;
}

/***
  * do some check before the upgrade
  *
  * buff : pointer the upload content
  * len  : the upload content length
  **/
BOOL onvif_tds_FirmwareUpgradeCheck(const char * buff, int len)
{
	// todo : add the check code ...

    // the firmware length is too short
    if (len < 100)
    {
        return FALSE;
    }
    
	return TRUE;
}

/***
  * begin firmware upgrade
  *
  * buff : pointer the upload content
  * len  : the upload content length
  **/
BOOL onvif_tds_FirmwareUpgrade(const char * buff, int len)
{
	// todo : add the upgrade code ...

	
	return TRUE;
}

/***
  * After the upgrade is complete do some works, such as reboot device ...
  *  
  **/
void onvif_tds_FirmwareUpgradePost()
{
	
}

BOOL onvif_tds_StartSystemRestore(const char * lip, uint32 lport, tds_StartSystemRestore_RES * p_res)
{
    // todo : do some file upload prepare ...
	
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		sprintf(p_res->UploadUri, "https://%s:%d/SystemRestore", lip, lport);
	}
	else
	{
		sprintf(p_res->UploadUri, "http://%s:%d/SystemRestore", lip, lport);
	}
#else
	sprintf(p_res->UploadUri, "http://%s:%d/SystemRestore", lip, lport);
#endif

	p_res->ExpectedDownTime = 5 * 60; 	// 5 minutes

	return TRUE;
}

/***
  * do some check before the restore
  *
  * buff : pointer the upload content
  * len  : the upload content length
  **/
BOOL onvif_tds_SystemRestoreCheck(const char * buff, int len)
{
	// todo : add the check code ...
	
	if (NULL == buff || len < 10)
	{
	    return FALSE;
	}
	
	return TRUE;
}

/***
  * begin system restore
  *
  * buff : pointer the upload content
  * len  : the upload content length
  **/
BOOL onvif_tds_SystemRestore(const char * buff, int len)
{
	// todo : add the system restore code ...

	
	return TRUE;
}

/***
  * After the system restore is complete do some works, such as reboot device ...
  *  
  **/
void onvif_tds_SystemRestorePost()
{
    // todo : please comment the code below
    // send onvif hello message, just for test
    sleep(3);
    onvif_hello();
}

#ifdef IPFILTER_SUPPORT	

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetIPAddressFilter(tds_SetIPAddressFilter_REQ * p_req)
{
	memcpy(&g_onvif_cfg.ipaddr_filter, &p_req->IPAddressFilter, sizeof(onvif_IPAddressFilter));

	// todo : add your handler code ...
	
	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_IPFilterListIsFull
 **/
ONVIF_RET onvif_tds_AddIPAddressFilter(tds_AddIPAddressFilter_REQ * p_req)
{
    int i;
    onvif_PrefixedIPAddress * p_item;
    
	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		if (onvif_is_ipaddr_filter_exist(g_onvif_cfg.ipaddr_filter.IPv4Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), &p_req->IPAddressFilter.IPv4Address[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address));
		if (p_item)
		{
			p_item->PrefixLength = p_req->IPAddressFilter.IPv4Address[i].PrefixLength;
			strcpy(p_item->Address, p_req->IPAddressFilter.IPv4Address[i].Address);
		}
		else
		{
			return ONVIF_ERR_IPFilterListIsFull;
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{	    
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		if (onvif_is_ipaddr_filter_exist(g_onvif_cfg.ipaddr_filter.IPv6Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), &p_req->IPAddressFilter.IPv6Address[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address));
		if (p_item)
		{
			p_item->PrefixLength = p_req->IPAddressFilter.IPv6Address[i].PrefixLength;
			strcpy(p_item->Address, p_req->IPAddressFilter.IPv6Address[i].Address);
		}
		else
		{
			return ONVIF_ERR_IPFilterListIsFull;
		}
	}

	// todo : add your handler code ...

	
	return ONVIF_OK;
}

/**
 * The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_NoIPv4Address
 **/
ONVIF_RET onvif_tds_RemoveIPAddressFilter(tds_RemoveIPAddressFilter_REQ * p_req)
{
    int i;
	onvif_PrefixedIPAddress * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), &p_req->IPAddressFilter.IPv4Address[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoIPv4Address;
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), &p_req->IPAddressFilter.IPv6Address[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoIPv6Address;
		}
	}

	// todo : add your handler code ...
	

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), &p_req->IPAddressFilter.IPv4Address[i]);
		if (NULL != p_item)
		{
			memset(p_item, 0, sizeof(onvif_PrefixedIPAddress));
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), &p_req->IPAddressFilter.IPv6Address[i]);
		if (NULL != p_item)
		{
			memset(p_item, 0, sizeof(onvif_PrefixedIPAddress));
		}
	}
    
	return ONVIF_OK;
}

#endif // end of IPFILTER_SUPPORT


