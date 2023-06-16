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

#include "onvif_doorcontrol.h"
#include "onvif.h"
#include "onvif_event.h"
#include "onvif_timer.h"

#ifdef PROFILE_C_SUPPORT

extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

/***************************************************************************************/

void onvif_DoorChangedNotify(DoorList * p_door)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Door/Changed", PropertyOperation_Changed, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_DoorRemovedNotify(DoorList * p_door)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Door/Removed", PropertyOperation_Deleted, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_AccessPointStateEnabledChangedNotify(AccessPointList * p_accesspoint)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:AccessPoint/State/Enabled", PropertyOperation_Changed, 
		"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
		"State", p_accesspoint->Enabled ? "true" : "false", NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_DoorStateDoorModeChangedNotify(DoorList * p_door)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Door/State/DoorMode", PropertyOperation_Changed, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		"State", onvif_DoorModeToString(p_door->DoorState.DoorMode), NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_DoorStataChanged(void * argv)
{
    DoorList * p_door = (DoorList *)argv;

    p_door->DoorState.DoorMode = DoorMode_Locked;
    
    onvif_DoorStateDoorModeChangedNotify(p_door);
}

/***************************************************************************************/

/**
 * GetAccessPointInfoList 
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAccessPointInfoList(tac_GetAccessPointInfoList_REQ * p_req, tac_GetAccessPointInfoList_RES * p_res)
{
    int nums = 0;
    AccessPointList * p_accesspoint = g_onvif_cfg.access_points;
    
    if (p_req->StartReferenceFlag)
    {
        p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->StartReference);
        if (NULL == p_accesspoint)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_accesspoint)
    {
        AccessPointList * p_accesspoint1 = onvif_add_AccessPoint(&p_res->AccessPointInfo);
        if (p_accesspoint1)
        {
            memcpy(&p_accesspoint1->AccessPointInfo, &p_accesspoint->AccessPointInfo, sizeof(onvif_AccessPointInfo));
        }
        
        p_accesspoint = p_accesspoint->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_accesspoint)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_accesspoint->AccessPointInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * EnableAccessPoint 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ONVIF_ERR_NotSupported
 */
ONVIF_RET onvif_tac_EnableAccessPoint(tac_EnableAccessPoint_REQ * p_req)
{
    AccessPointList * p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->Token);
    if (NULL == p_accesspoint)
    {
        return ONVIF_ERR_NotFound;
    }

    p_accesspoint->Enabled = TRUE;

    // send event topic tns1:AccessPoint/State/Enabled notify
    onvif_AccessPointStateEnabledChangedNotify(p_accesspoint);	

    return ONVIF_OK;
}

/**
 * DisableAccessPoint 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ONVIF_ERR_NotSupported
 */
ONVIF_RET onvif_tac_DisableAccessPoint(tac_DisableAccessPoint_REQ * p_req)
{
    AccessPointList * p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->Token);
    if (NULL == p_accesspoint)
    {
        return ONVIF_ERR_NotFound;
    }

    p_accesspoint->Enabled = FALSE;

    // send event topic tns1:AccessPoint/State/Enabled notify
    onvif_AccessPointStateEnabledChangedNotify(p_accesspoint);	
    
    return ONVIF_OK;
}

/**
 * GetAreaInfoList 
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAreaInfoList(tac_GetAreaInfoList_REQ * p_req, tac_GetAreaInfoList_RES * p_res)
{
    int nums = 0;
    AreaInfoList * p_area = g_onvif_cfg.area_info;
    
    if (p_req->StartReferenceFlag)
    {
        p_area = onvif_find_AreaInfo(g_onvif_cfg.area_info, p_req->StartReference);
        if (NULL == p_area)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_area)
    {
        AreaInfoList * p_info = onvif_add_AreaInfo(&p_res->AreaInfo);
        if (p_info)
        {
            memcpy(&p_info->AreaInfo, &p_area->AreaInfo, sizeof(onvif_AreaInfo));
        }
        
        p_area = p_area->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_area)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_area->AreaInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * GetDoorList 
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tdc_GetDoorList(tdc_GetDoorList_REQ * p_req, tdc_GetDoorList_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;
    
    if (p_req->StartReferenceFlag)
    {
        p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->StartReference);
        if (NULL == p_door)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_door)
    {
        DoorList * p_info = onvif_add_Door(&p_res->Door);
        if (p_info)
        {
            memcpy(&p_info->Door, &p_door->Door, sizeof(onvif_Door));
        }
        
        p_door = p_door->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_door)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_door->Door.DoorInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * CreateDoor 
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxDoors or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tdc_CreateDoor(tdc_CreateDoor_REQ * p_req, tdc_CreateDoor_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;

    if (p_req->Door.DoorInfo.token[0] != '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }
    
    while (p_door)
    {
        nums++;
        p_door = p_door->next;
    }

    if (nums >= g_onvif_cfg.Capabilities.doorcontrol.MaxDoors)
    {
        return ONVIF_ERR_MaxDoors; 
    }
    
    p_door = onvif_add_Door(&g_onvif_cfg.doors);
    if (p_door)
    {
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
        sprintf(p_door->Door.DoorInfo.token, "DOOR_TOKEN_%d", ++g_onvif_cls.door_idx);
    }

    // todo : add your hander code ...


    strcpy(p_res->Token, p_door->Door.DoorInfo.token);

    onvif_DoorChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * SetDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxDoors or ONVIF_ERR_ReferenceNotFound or ONVIF_ERR_ClientSuppliedTokenSupported
 */
ONVIF_RET onvif_tdc_SetDoor(tdc_SetDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Door.DoorInfo.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Door.DoorInfo.token);
    if (p_door)
    {
        // update
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
    }
    else // add new
    {
        int nums = 0;
        p_door = g_onvif_cfg.doors;
        while (p_door)
        {
            nums++;
            p_door = p_door->next;
        }

        if (nums >= g_onvif_cfg.Capabilities.doorcontrol.MaxDoors)
        {
            return ONVIF_ERR_MaxDoors; 
        }
        
        p_door = onvif_add_Door(&g_onvif_cfg.doors);
        if (p_door)
        {
            memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
        }
    }

    // todo : add your hander code ...


    onvif_DoorChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * ModifyDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound
 */
ONVIF_RET onvif_tdc_ModifyDoor(tdc_ModifyDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Door.DoorInfo.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Door.DoorInfo.token);
    if (p_door)
    {
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : add your hander code ...

    
    onvif_DoorChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * DeleteDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceInUse
 */
ONVIF_RET onvif_tdc_DeleteDoor(tdc_DeleteDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (p_door)
    {
        // todo : add your hander code ...

        
        onvif_DoorRemovedNotify(p_door);
        
        onvif_free_Door(&g_onvif_cfg.doors, p_door);
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }
    
    return ONVIF_OK;
}

/**
 * GetDoorInfoList 
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tdc_GetDoorInfoList(tdc_GetDoorInfoList_REQ * p_req, tdc_GetDoorInfoList_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;
    
    if (p_req->StartReferenceFlag)
    {
        p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->StartReference);
        if (NULL == p_door)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_door)
    {
        DoorInfoList * p_info = onvif_add_DoorInfo(&p_res->DoorInfo);
        if (p_info)
        {
            memcpy(&p_info->DoorInfo, &p_door->Door.DoorInfo, sizeof(onvif_DoorInfo));
        }
        
        p_door = p_door->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_door)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_door->Door.DoorInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * AccessDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_AccessDoor(tdc_AccessDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Access == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do access door operation ...
    p_door->DoorState.DoorMode = DoorMode_Accessed;
    
    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);

    timer_add(3, onvif_DoorStataChanged, p_door, TIMER_MODE_SINGLE);
    
    return ONVIF_OK;
}

/**
 * LockDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDoor(tdc_LockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Lock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Locked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * LockDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_UnlockDoor(tdc_UnlockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Unlock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Unlocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * DoubleLockDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_DoubleLockDoor(tdc_DoubleLockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.DoubleLock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_DoubleLocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * BlockDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_BlockDoor(tdc_BlockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Block == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Blocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * LockDownDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDownDoor(tdc_LockDownDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockDown == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_LockedDown;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * LockDownReleaseDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDownReleaseDoor(tdc_LockDownReleaseDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockDown == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Locked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * LockOpenDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockOpenDoor(tdc_LockOpenDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockOpen == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_LockedOpen;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * LockOpenReleaseDoor 
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockOpenReleaseDoor(tdc_LockOpenReleaseDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockOpen == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Unlocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

#endif // PROFILE_C_SUPPORT



