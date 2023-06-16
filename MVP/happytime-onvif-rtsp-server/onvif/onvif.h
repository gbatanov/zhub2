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

#ifndef	__H_ONVIF_H__
#define	__H_ONVIF_H__

#include "sys_inc.h"
#include "onvif_cm.h"
#include "linked_list.h"
#include "hqueue.h"

/***************************************************************************************/

#define ONVIF_MSG_SRC	    1
#define ONVIF_TIMER_SRC	    2
#define ONVIF_DEL_UA_SRC    3
#define ONVIF_EXIT          4

/***************************************************************************************/

// video source list
typedef struct _VideoSourceList
{
	struct _VideoSourceList * next;
	
    onvif_VideoSource       VideoSource; 
    onvif_VideoSourceMode	VideoSourceMode;

    ImagingPresetList     * Presets;
    char                    CurrentPrestToken[ONVIF_TOKEN_LEN];
    
#ifdef THERMAL_SUPPORT
    BOOL                                    ThermalSupport;
    onvif_ThermalConfiguration              ThermalConfiguration;
    onvif_ThermalConfigurationOptions       ThermalConfigurationOptions;
    onvif_RadiometryConfiguration           RadiometryConfiguration;
    onvif_RadiometryConfigurationOptions    RadiometryConfigurationOptions;
#endif
} VideoSourceList;

// video source mode list
typedef struct _VideoSourceModeList
{
    struct _VideoSourceModeList * next;

	onvif_VideoSourceMode	VideoSourceMode;  	
} VideoSourceModeList;

// video source configuration list
typedef struct _VideoSourceConfigurationList
{
	struct _VideoSourceConfigurationList * next;

	onvif_VideoSourceConfiguration Configuration; 
} VideoSourceConfigurationList;

// video encoder configuration list
typedef struct _VideoEncoderConfigurationList
{    
	struct _VideoEncoderConfigurationList * next;	

	onvif_VideoEncoderConfiguration	Configuration;	
} VideoEncoderConfigurationList;

// audio source list
typedef struct _AudioSourceList
{    
	struct _AudioSourceList * next;
	
    onvif_AudioSource AudioSource;	
} AudioSourceList;

// audio source configuration list
typedef struct _AudioSourceConfigurationList
{    
	struct _AudioSourceConfigurationList * next;
	
    onvif_AudioSourceConfiguration 	Configuration;	
} AudioSourceConfigurationList;

// audio encoder configuration list
typedef struct _AudioEncoderConfigurationList
{
	struct _AudioEncoderConfigurationList * next;
	
    onvif_AudioEncoderConfiguration Configuration;
} AudioEncoderConfigurationList;

typedef struct _MetadataConfigurationList
{
	struct _MetadataConfigurationList * next;
	
	onvif_MetadataConfiguration Configuration;
} MetadataConfigurationList;

// ptz preset list
typedef struct _PTZPresetList
{
    struct _PTZPresetList * next;
    
    onvif_PTZPreset	PTZPreset;
} PTZPresetList;

// ptz configuration list
typedef struct _PTZConfigurationList
{
	struct _PTZConfigurationList * next;
	
	onvif_PTZConfiguration  Configuration;
} PTZConfigurationList;

// ptz node list
typedef struct _PTZNodeList
{
	struct _PTZNodeList * next;

	onvif_PTZNode	PTZNode;	
} PTZNodeList;

// preset tour list
typedef struct _PresetTourList
{
	struct _PresetTourList * next;

	onvif_PresetTour    PresetTour;
} PresetTourList;

// video analytics configuration list
typedef struct _VideoAnalyticsConfigurationList
{
	struct _VideoAnalyticsConfigurationList	* next;

	onvif_SupportedRules                SupportedRules;			    // supported rules
	onvif_SupportedAnalyticsModules     SupportedAnalyticsModules;  // supported analytics modules
	
	onvif_VideoAnalyticsConfiguration   Configuration;
} VideoAnalyticsConfigurationList;

// network interface list
typedef struct _NetworkInterfaceList
{
	struct _NetworkInterfaceList * next;
	
	onvif_NetworkInterface	NetworkInterface; 
} NetworkInterfaceList;

typedef struct 
{
	onvif_NetworkProtocol		    NetworkProtocol;
	onvif_DNSInformation		    DNSInformation;
	onvif_NTPInformation		    NTPInformation;
	onvif_HostnameInformation	    HostnameInformation;
	onvif_NetworkGateway		    NetworkGateway;
	onvif_DiscoveryMode			    DiscoveryMode;
	onvif_NetworkZeroConfiguration  ZeroConfiguration;
	onvif_DynamicDNSInformation     DynamicDNSInformation;
	
	NetworkInterfaceList          * interfaces;
} ONVIF_NET;

// osd configuration list
typedef struct _OSDConfigurationList
{
	struct _OSDConfigurationList * next;
	
	onvif_OSDConfiguration OSD;
} OSDConfigurationList;

typedef struct _RecordingList
{
	struct _RecordingList * next;

	onvif_Recording	Recording;

	time_t	EarliestRecording;
	time_t	LatestRecording;
	
	onvif_RecordingStatus	RecordingStatus;
} RecordingList;

typedef struct _RecordingJobList
{	
	struct _RecordingJobList * next;

	onvif_RecordingJob	RecordingJob;
} RecordingJobList;

typedef struct _NotificationMessageList
{
	struct _NotificationMessageList * next;

    int     refcnt;     // reference count

	onvif_NotificationMessage	NotificationMessage;
} NotificationMessageList;

typedef struct _ONVIF_PROFILE
{
    struct _ONVIF_PROFILE * next;
	
    VideoSourceConfigurationList    * v_src_cfg;        // video source configuration
    VideoEncoder2ConfigurationList  * v_enc_cfg;        // video encoder configuration
    
#ifdef AUDIO_SUPPORT    
	AudioSourceConfigurationList    * a_src_cfg;        // audio source configuration
	AudioEncoder2ConfigurationList  * a_enc_cfg;        // audio encoder configuration
	AudioDecoderConfigurationList   * a_dec_cfg;        // audio decoder configuration
#endif

#ifdef PTZ_SUPPORT
	PTZConfigurationList            * ptz_cfg;          // ptz configuration
	PTZPresetList                   * presets;          // ptz presets list
	PresetTourList                  * preset_tour;      // preset tour
#endif

	MetadataConfigurationList       * metadata_cfg;     // metadata configuration

#ifdef VIDEO_ANALYTICS
	VideoAnalyticsConfigurationList * va_cfg;	        // video analytics configuration
#endif

#ifdef DEVICEIO_SUPPORT
    AudioOutputConfigurationList    * a_output_cfg;     // audio output configuration
#endif

    char        name[ONVIF_NAME_LEN];				    // profile name
    char        token[ONVIF_TOKEN_LEN];				    // profile token
    char        stream_uri[ONVIF_URI_LEN];			    // rtsp stream address
    BOOL        fixed;							        // fixed profile flag	
    BOOL        multicasting;						    // sending multicast streaming flag
} ONVIF_PROFILE;

typedef struct
{
    char        serv_ip[128];					        // The Config service address
	uint16 	    serv_port;						        // The Config service port
} ONVIF_SRV;

typedef struct 
{
	uint32 	    need_auth	 	: 1;			        // Whether need auth request flag
	uint32	    evt_sim_flag 	: 1;			        // event simulate flag
	uint32      log_enable      : 1;			        // Whether log enable flag
	uint32 	    https_enable	: 1;				    // whether https enable flag
	uint32	    reserved     	: 28;

    int         servs_num;                              // the servers number
    ONVIF_SRV   servs[MAX_SERVERS];
	int         http_max_users;                         // max http connection clients        
	int         log_level;                              // log level, ses sys_log.h
	
	onvif_User  users[MAX_USERS];	
	onvif_Scope scopes[MAX_SCOPE_NUMS];
	ONVIF_NET   network;
	
	char        EndpointReference[64];
	
	int         evt_renew_time;
	int         evt_sim_interval;				        // event simulate interval 

#ifdef PROFILE_Q_SUPPORT
    int         device_state;                           // 0 - Factory Default state, 1 - Operational State
                                                        // Factory Default State requires WS-Discovery, DHCP and IPv4 Link Local Address to be enabled by default
                                                        // A device shall provide full anonymous access to all ONVIF commands while the device operates in Factory Default State
#endif

	/********************************************************/
	VideoSourceList   		              * v_src;	
	VideoSourceConfigurationList	      * v_src_cfg;
	VideoEncoder2ConfigurationList	      * v_enc_cfg;	

#ifdef AUDIO_SUPPORT	
	AudioSourceList   		              * a_src;
	AudioSourceConfigurationList 	      * a_src_cfg;
	AudioEncoder2ConfigurationList        * a_enc_cfg;
	AudioDecoderConfigurationList         * a_dec_cfg;
#endif

	ONVIF_PROFILE 		                  * profiles;	
	OSDConfigurationList                  * OSDs;
	MetadataConfigurationList	          * metadata_cfg;

#ifdef MEDIA2_SUPPORT
    MaskList                              * mask;
    onvif_MaskOptions                       MaskOptions;
#endif

#ifdef PTZ_SUPPORT
	PTZNodeList                           * ptz_node;
	PTZConfigurationList	              * ptz_cfg;
#endif

#ifdef VIDEO_ANALYTICS
	VideoAnalyticsConfigurationList       * va_cfg;	
#endif

#ifdef PROFILE_G_SUPPORT
	RecordingList                         * recordings;
	RecordingJobList                      * recording_jobs;
	int 	                                replay_session_timeout;
#endif

#ifdef PROFILE_C_SUPPORT
    AccessPointList						  * access_points;
    DoorList                              * doors;
    AreaInfoList                          * area_info;
#endif

#ifdef DEVICEIO_SUPPORT
	VideoOutputList						  * v_output;
	VideoOutputConfigurationList          * v_output_cfg;
	AudioOutputList                       * a_output;
	AudioOutputConfigurationList          * a_output_cfg;
	RelayOutputList                       * relay_output;
	DigitalInputList                      * digit_input;
	SerialPortList                        * serial_port;
	
	onvif_AudioOutputConfigurationOptions   AudioOutputConfigurationOptions;
	onvif_RelayOutputOptions                RelayOutputOptions;
	onvif_DigitalInputConfigurationOptions  DigitalInputConfigurationOptions;
#endif

	/********************************************************/
	onvif_DeviceInformation					DeviceInformation;
	onvif_ImagingSettings					ImagingSettings;
	onvif_ImagingOptions					ImagingOptions;
	onvif_Capabilities						Capabilities;
	onvif_SystemDateTime					SystemDateTime;
	onvif_RemoteUser                        RemoteUser;
	
	onvif_VideoSourceConfigurationOptions   VideoSourceConfigurationOptions;
	onvif_VideoEncoderConfigurationOptions  VideoEncoderConfigurationOptions;
	
	VideoEncoder2ConfigurationOptionsList * v_enc_cfg_opt;

#ifdef AUDIO_SUPPORT	
	AudioEncoder2ConfigurationOptionsList * a_enc_cfg_opt;
#endif

#ifdef PTZ_SUPPORT
	onvif_PTZConfigurationOptions           PTZConfigurationOptions;
#endif

	onvif_MetadataConfigurationOptions		MetadataConfigurationOptions;
	onvif_OSDConfigurationOptions			OSDConfigurationOptions;

#ifdef CREDENTIAL_SUPPORT
    CredentialList                        * credential;
#endif

#ifdef ACCESS_RULES
    AccessProfileList                     * access_rules;
#endif

#ifdef SCHEDULE_SUPPORT
    ScheduleList                          * schedule;
    SpecialDayGroupList                   * specialdaygroup;
#endif

#ifdef RECEIVER_SUPPORT
    ReceiverList                          * receiver;
#endif

#ifdef IPFILTER_SUPPORT
    onvif_IPAddressFilter                   ipaddr_filter;
#endif
} ONVIF_CFG;


typedef struct
{
	uint32      sys_timer_run	: 1;    // timer running flag
	uint32      discovery_flag	: 1;    // discovery flag
	uint32      reserved		: 30;

    int         rtsp_port;              // rtsp server port
    
#ifdef MEDIA2_SUPPORT
    int         mask_idx;               // mask index
#endif

#ifdef AUDIO_SUPPORT
	int         a_src_idx;              // audio source index
	int         a_enc_idx;              // audio encoder index
	int         a_dec_idx;              // audio decoder index
#endif

	int         v_src_idx;              // video source index	
	int         v_enc_idx;              // video encoder index	
	int         profile_idx;	        // profile index
	int         netinf_idx;             // network interface index
	int         osd_idx;                // osd index

#ifdef PTZ_SUPPORT
    int         preset_idx;             // preset index
    int         preset_tour_idx;        // preset tour index
#endif

#ifdef PROFILE_G_SUPPORT
	int         recording_idx;          // recording index
	int         recordingjob_idx;       // recording job index
	int         track_idx;              // track index
#endif

#ifdef PROFILE_C_SUPPORT
    int         aceess_point_idx;       // access point info index
    int         door_idx;               // door info index
    int         area_idx;               // area info index
#endif

#ifdef PROFILE_G_SUPPORT
    int         search_idx;             // search index
    LINKED_LIST * search_list;          // search ua list
#endif

#ifdef DEVICEIO_SUPPORT
    int         v_out_idx;              // video output index
    int         v_out_cfg_idx;          // video output configuration index
    int         a_out_idx;              // audio output index
    int         a_out_cfg_idx;          // audio output configuration index
    int         relay_idx;              // relay output index
    int         digit_input_idx;        // digit input index
    int         serial_port_idx;        // serial port index
#endif

#ifdef CREDENTIAL_SUPPORT
    int         credential_idx;         // credential index
#endif

#ifdef ACCESS_RULES
    int         accessrule_idx;         // accessrule index
#endif

#ifdef SCHEDULE_SUPPORT
    int         schedule_idx;           // schedule index
    int         specialdaygroup_idx;    // special day group index
#endif

#ifdef RECEIVER_SUPPORT
    int         receiver_idx;           // receiver index
#endif

#ifdef VIDEO_ANALYTICS
    int         va_idx;                 // video analytics index
#endif

	PPSN_CTX  * eua_fl;                 // event subscriber free list    
	PPSN_CTX  * eua_ul;                 // event subscriber used list  
	
	uint32      timer_id;               // timer id

	SOCKET      discovery_fd;           // discovery socket handler
	pthread_t   discovery_tid;          // discovery task handler

    HQUEUE *    msg_queue;              // message receive queue
	pthread_t   tid_main;               // main task thread
} ONVIF_CLS;

typedef struct onvif_internal_msg
{
	char *      msg_dua;		        // message destination unit
	uint32	    msg_evt;		        // event / command value
	uint32	    msg_src;		        // message type
	int	        msg_len;		        // message buffer length
	char *      msg_buf;		        // message buffer
} OIMSG;


#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************************/
HT_API BOOL                                      onvif_is_scope_exist(const char * scope);
HT_API ONVIF_RET                                 onvif_add_scope(const char * scope, BOOL fixed);
HT_API onvif_Scope *                             onvif_find_scope(const char * scope);
HT_API onvif_Scope *                             onvif_get_idle_scope();

/***************************************************************************************/
HT_API BOOL                                      onvif_is_user_exist(const char * username);
HT_API ONVIF_RET                                 onvif_add_user(onvif_User * p_user);
HT_API onvif_User *                              onvif_find_user(const char * username);
HT_API onvif_User *                              onvif_get_idle_user();
HT_API const char *                              onvif_get_user_pass(const char * username);

/***************************************************************************************/
HT_API ONVIF_PROFILE *                           onvif_add_profile(BOOL fixed);
HT_API ONVIF_PROFILE *                           onvif_find_profile(ONVIF_PROFILE * p_head, const char * token);
HT_API void                                      onvif_free_profiles(ONVIF_PROFILE ** p_head);

HT_API VideoSourceList *                         onvif_add_VideoSource(int w, int h);
HT_API VideoSourceList *                         onvif_find_VideoSource(VideoSourceList * p_head, const char * token);
HT_API VideoSourceList *                         onvif_find_VideoSource_by_size(VideoSourceList * p_head, int w, int h);
HT_API void                                      onvif_free_VideoSources(VideoSourceList ** p_head);

HT_API VideoSourceConfigurationList *            onvif_add_VideoSourceConfiguration(int w, int h);
HT_API VideoSourceConfigurationList *            onvif_find_VideoSourceConfiguration(VideoSourceConfigurationList * p_head, const char * token);
HT_API VideoSourceConfigurationList *            onvif_find_VideoSourceConfiguration_by_size(VideoSourceConfigurationList * p_head, int w, int h);
HT_API void                                      onvif_free_VideoSourceConfigurations(VideoSourceConfigurationList ** p_head);

HT_API VideoEncoder2ConfigurationList *          onvif_add_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_node);
HT_API VideoEncoder2ConfigurationList *          onvif_find_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_head, const char * token);
HT_API VideoEncoder2ConfigurationList *          onvif_find_VideoEncoder2Configuration_by_param(VideoEncoder2ConfigurationList * p_head, VideoEncoder2ConfigurationList * p_node);
HT_API void                                      onvif_free_VideoEncoder2Configurations(VideoEncoder2ConfigurationList ** p_head);

HT_API NetworkInterfaceList *                    onvif_add_NetworkInterface();
HT_API NetworkInterfaceList *                    onvif_find_NetworkInterface(NetworkInterfaceList * p_head, const char * token);
HT_API void                                      onvif_free_NetworkInterfaces(NetworkInterfaceList ** p_head);

HT_API OSDConfigurationList *                    onvif_add_OSDConfiguration();
HT_API OSDConfigurationList *                    onvif_find_OSDConfiguration(OSDConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_OSDConfigurations(OSDConfigurationList ** p_head);

HT_API MetadataConfigurationList *               onvif_add_MetadataConfiguration(MetadataConfigurationList ** p_head);
HT_API MetadataConfigurationList *               onvif_find_MetadataConfiguration(MetadataConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_MetadataConfigurations(MetadataConfigurationList ** p_head);

HT_API VideoEncoder2ConfigurationOptionsList *   onvif_add_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head);
HT_API VideoEncoder2ConfigurationOptionsList *   onvif_find_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList * p_head, const char * encoding);
HT_API void                                      onvif_free_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head);

/***************************************************************************************/
HT_API NotificationMessageList *                 onvif_add_NotificationMessage(NotificationMessageList ** p_head);
HT_API void                                      onvif_free_NotificationMessage(NotificationMessageList * p_message);
HT_API void                                      onvif_free_NotificationMessages(NotificationMessageList ** p_head);

HT_API SimpleItemList *                          onvif_add_SimpleItem(SimpleItemList ** p_head);
HT_API void                                      onvif_free_SimpleItems(SimpleItemList ** p_head);

HT_API ElementItemList *                         onvif_add_ElementItem(ElementItemList ** p_head);
HT_API void                                      onvif_free_ElementItems(ElementItemList ** p_head);

/***************************************************************************************/

#ifdef IMAGE_SUPPORT
HT_API ImagingPresetList *                       onvif_add_ImagingPreset(ImagingPresetList ** p_head);
HT_API ImagingPresetList *                       onvif_find_ImagingPreset(ImagingPresetList * p_head, const char * token);
HT_API void                                      onvif_free_ImagingPresets(ImagingPresetList ** p_head);
#endif

/***************************************************************************************/

#ifdef AUDIO_SUPPORT
HT_API AudioSourceList *                         onvif_add_AudioSource(AudioSourceList ** p_head);
HT_API AudioSourceList *                         onvif_find_AudioSource(AudioSourceList * p_head, const char * token);
HT_API void                                      onvif_free_AudioSources(AudioSourceList ** p_head);

HT_API AudioSourceConfigurationList *            onvif_add_AudioSourceConfiguration();
HT_API AudioSourceConfigurationList *            onvif_find_AudioSourceConfiguration(AudioSourceConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioSourceConfigurations(AudioSourceConfigurationList ** p_head);

HT_API AudioEncoder2ConfigurationList *          onvif_add_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_node);
HT_API AudioEncoder2ConfigurationList *          onvif_find_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_head, const char * token);
HT_API AudioEncoder2ConfigurationList *          onvif_find_AudioEncoder2Configuration_by_param(AudioEncoder2ConfigurationList * p_head, AudioEncoder2ConfigurationList * p_node);
HT_API void                                      onvif_free_AudioEncoder2Configurations(AudioEncoder2ConfigurationList ** p_head);

HT_API AudioEncoder2ConfigurationOptionsList *   onvif_add_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head);
HT_API AudioEncoder2ConfigurationOptionsList *   onvif_find_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList * p_head, const char * encoding);
HT_API void                                      onvif_free_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head);

HT_API AudioDecoderConfigurationList *           onvif_add_AudioDecoderConfiguration();
HT_API AudioDecoderConfigurationList *           onvif_find_AudioDecoderConfiguration(AudioDecoderConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioDecoderConfigurations(AudioDecoderConfigurationList ** p_head);
#endif

/***************************************************************************************/

#ifdef MEDIA2_SUPPORT
HT_API MaskList *                                onvif_add_Mask(MaskList ** p_head);
HT_API MaskList *                                onvif_find_Mask(MaskList * p_head, const char * token);
HT_API void                                      onvif_free_Masks(MaskList ** p_head);
#endif

/***************************************************************************************/

#ifdef PTZ_SUPPORT
HT_API PTZNodeList *                             onvif_add_PTZNode(PTZNodeList ** p_head);
HT_API PTZNodeList *                             onvif_find_PTZNode(PTZNodeList * p_head, const char * token);
HT_API void                                      onvif_free_PTZNodes(PTZNodeList ** p_head);

HT_API PTZConfigurationList *                    onvif_add_PTZConfiguration(PTZConfigurationList ** p_head);
HT_API PTZConfigurationList *                    onvif_find_PTZConfiguration(PTZConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_PTZConfigurations(PTZConfigurationList ** p_head);

HT_API PTZPresetList *                           onvif_add_PTZPreset(PTZPresetList ** p_head);
HT_API PTZPresetList *                           onvif_find_PTZPreset(PTZPresetList * p_head, const char  * preset_token);
HT_API void                                      onvif_free_PTZPreset(PTZPresetList ** p_head, PTZPresetList * p_node);
HT_API void                                      onvif_free_PTZPresets(PTZPresetList ** p_head);

HT_API PTZPresetTourSpotList *                   onvif_add_PTZPresetTourSpot(PTZPresetTourSpotList ** p_head);
HT_API void                                      onvif_free_PTZPresetTourSpots(PTZPresetTourSpotList ** p_head);

HT_API PresetTourList *                          onvif_add_PresetTour(PresetTourList ** p_head);
HT_API PresetTourList *                          onvif_find_PresetTour(PresetTourList * p_head, const char * token);
HT_API void                                      onvif_free_PresetTour(PresetTourList ** p_head, PresetTourList * p_node);
HT_API void                                      onvif_free_PresetTours(PresetTourList ** p_head);
HT_API int                                       onvif_count_PresetTours(PresetTourList * p_head);
#endif

/***************************************************************************************/

#ifdef VIDEO_ANALYTICS
HT_API ConfigList *                              onvif_add_Config(ConfigList ** p_head);
HT_API void                                      onvif_free_Config(ConfigList * p_node);
HT_API void                                      onvif_free_Configs(ConfigList ** p_head);
HT_API ConfigList *                              onvif_find_Config(ConfigList * p_head, const char * name);
HT_API void                                      onvif_remove_Config(ConfigList ** p_head, ConfigList * p_remove);
HT_API ConfigList *                              onvif_get_prev_Config(ConfigList * p_head, ConfigList * p_found);

HT_API ConfigDescriptionList *                   onvif_add_ConfigDescription(ConfigDescriptionList ** p_head);
HT_API void                                      onvif_free_ConfigDescriptions(ConfigDescriptionList ** p_head);

HT_API ConfigDescription_MessagesList *          onvif_add_ConfigDescription_Message(ConfigDescription_MessagesList ** p_head);
HT_API void                                      onvif_free_ConfigDescription_Message(ConfigDescription_MessagesList * p_item);
HT_API void                                      onvif_free_ConfigDescription_Messages(ConfigDescription_MessagesList ** p_head);

HT_API ConfigOptionsList *                       onvif_add_ConfigOptions(ConfigOptionsList ** p_head);
HT_API void                                      onvif_free_ConfigOptions(ConfigOptionsList ** p_head);

HT_API SimpleItemDescriptionList *               onvif_add_SimpleItemDescription(SimpleItemDescriptionList ** p_head);
HT_API void                                      onvif_free_SimpleItemDescriptions(SimpleItemDescriptionList ** p_head);

HT_API VideoAnalyticsConfigurationList *         onvif_add_VideoAnalyticsConfiguration();
HT_API VideoAnalyticsConfigurationList *         onvif_find_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_VideoAnalyticsConfigurations(VideoAnalyticsConfigurationList ** p_head);
#endif

/***************************************************************************************/

#ifdef PROFILE_G_SUPPORT
HT_API RecordingList *                           onvif_add_Recording();
HT_API RecordingList *                           onvif_find_Recording(RecordingList * p_head, const char * token);
HT_API void                                      onvif_free_Recording(RecordingList ** p_head, RecordingList * p_node);
HT_API void                                      onvif_free_Recordings(RecordingList ** p_head);

HT_API TrackList *                               onvif_add_Track(TrackList ** p_head);
HT_API void                                      onvif_free_Track(TrackList ** p_head, TrackList * p_track);
HT_API void                                      onvif_free_Tracks(TrackList ** p_head);
HT_API TrackList *                               onvif_find_Track(TrackList * p_head, const char * token);
HT_API int	                                     onvif_get_track_nums_by_type(TrackList * p_head, onvif_TrackType type);

HT_API RecordingJobList *                        onvif_add_RecordingJob();
HT_API RecordingJobList *                        onvif_find_RecordingJob(RecordingJobList * p_head, const char * token);
HT_API void                                      onvif_free_RecordingJob(RecordingJobList ** p_head, RecordingJobList * p_node);
HT_API void                                      onvif_free_RecordingJobs(RecordingJobList ** p_head);

HT_API RecordingInformationList *                onvif_add_RecordingInformation(RecordingInformationList ** p_head);
HT_API void                                      onvif_free_RecordingInformations(RecordingInformationList ** p_head);

HT_API FindEventResultList *                     onvif_add_FindEventResult(FindEventResultList ** p_head);
HT_API void                                      onvif_free_FindEventResult(FindEventResultList ** p_head, FindEventResultList * p_node);
HT_API void                                      onvif_free_FindEventResults(FindEventResultList ** p_head);

HT_API FindMetadataResultList *                  onvif_add_FindMetadataResult(FindMetadataResultList ** p_head);
HT_API void                                      onvif_free_FindMetadataResult(FindMetadataResultList ** p_head, FindMetadataResultList * p_node);
HT_API void                                      onvif_free_FindMetadataResults(FindMetadataResultList ** p_head);

HT_API FindPTZPositionResultList *               onvif_add_FindPTZPositionResult(FindPTZPositionResultList ** p_head);
HT_API void                                      onvif_free_FindPTZPositionResult(FindPTZPositionResultList ** p_head, FindPTZPositionResultList * p_node);
HT_API void                                      onvif_free_FindPTZPositionResults(FindPTZPositionResultList ** p_head);
#endif

/***************************************************************************************/

#ifdef PROFILE_C_SUPPORT
HT_API AccessPointList *                         onvif_add_AccessPoint(AccessPointList ** p_head);
HT_API AccessPointList *                         onvif_find_AccessPoint(AccessPointList * p_head, const char * token);
HT_API void                                      onvif_free_AccessPoint(AccessPointList ** p_head, AccessPointList * p_node);
HT_API void                                      onvif_free_AccessPoints(AccessPointList ** p_head);

HT_API DoorInfoList *                            onvif_add_DoorInfo(DoorInfoList ** p_head);
HT_API DoorInfoList *                            onvif_find_DoorInfo(DoorInfoList * p_head, const char * token);
HT_API void                                      onvif_free_DoorInfos(DoorInfoList ** p_head);

HT_API DoorList *                                onvif_add_Door(DoorList ** p_head);
HT_API DoorList *                                onvif_find_Door(DoorList * p_head, const char * token);
HT_API void                                      onvif_free_Door(DoorList ** p_head, DoorList * p_node);
HT_API void                                      onvif_free_Doors(DoorList ** p_head);

HT_API AreaInfoList *                            onvif_add_AreaInfo(AreaInfoList ** p_head);
HT_API AreaInfoList *                            onvif_find_AreaInfo(AreaInfoList * p_head, const char * token);
HT_API void                                      onvif_free_AreaInfo(AreaInfoList ** p_head, AreaInfoList * p_node);
HT_API void                                      onvif_free_AreaInfos(AreaInfoList ** p_head);
#endif // end of PROFILE_C_SUPPORT

/***************************************************************************************/

#ifdef DEVICEIO_SUPPORT
HT_API PaneLayoutList *                          onvif_add_PaneLayout(PaneLayoutList ** p_head);
HT_API PaneLayoutList *                          onvif_find_PaneLayout(PaneLayoutList * p_head, const char * token);
HT_API void                                      onvif_free_PaneLayouts(PaneLayoutList ** p_head);

HT_API VideoOutputList *                         onvif_add_VideoOutput(VideoOutputList ** p_head);
HT_API VideoOutputList *                         onvif_find_VideoOutput(VideoOutputList * p_head, const char * token);
HT_API void                                      onvif_free_VideoOutputs(VideoOutputList ** p_head);

HT_API VideoOutputConfigurationList *            onvif_add_VideoOutputConfiguration(VideoOutputConfigurationList ** p_head);
HT_API VideoOutputConfigurationList *            onvif_find_VideoOutputConfiguration(VideoOutputConfigurationList * p_head, const char * token);
HT_API VideoOutputConfigurationList *            onvif_find_VideoOutputConfiguration_by_OutputToken(VideoOutputConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_VideoOutputConfigurations(VideoOutputConfigurationList ** p_head);

HT_API AudioOutputList *                         onvif_add_AudioOutput(AudioOutputList ** p_head);
HT_API AudioOutputList *                         onvif_find_AudioOutput(AudioOutputList * p_head, const char * token);
HT_API void                                      onvif_free_AudioOutputs(AudioOutputList ** p_head);

HT_API AudioOutputConfigurationList *            onvif_add_AudioOutputConfiguration(AudioOutputConfigurationList ** p_head);
HT_API AudioOutputConfigurationList *            onvif_find_AudioOutputConfiguration(AudioOutputConfigurationList * p_head, const char * token);
HT_API AudioOutputConfigurationList *            onvif_find_AudioOutputConfiguration_by_OutputToken(AudioOutputConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioOutputConfigurations(AudioOutputConfigurationList ** p_head);

HT_API RelayOutputList *                         onvif_add_RelayOutput(RelayOutputList ** p_head);
HT_API RelayOutputList *                         onvif_find_RelayOutput(RelayOutputList * p_head, const char * token);
HT_API void                                      onvif_free_RelayOutputs(RelayOutputList ** p_head);

HT_API DigitalInputList *                        onvif_add_DigitalInput(DigitalInputList ** p_head);
HT_API DigitalInputList *                        onvif_find_DigitalInput(DigitalInputList * p_head, const char * token);
HT_API void                                      onvif_free_DigitalInputs(DigitalInputList ** p_head);

HT_API SerialPortList *                          onvif_add_SerialPort(SerialPortList ** p_head);
HT_API SerialPortList *                          onvif_find_SerialPort(SerialPortList * p_head, const char * token);
HT_API SerialPortList *                          onvif_find_SerialPort_by_ConfigurationToken(SerialPortList * p_head, const char * token);
HT_API void                                      onvif_free_SerialPorts(SerialPortList ** p_head);
HT_API void                                      onvif_malloc_SerialData(onvif_SerialData * p_data, int union_SerialData, int size);
HT_API void                                      onvif_free_SerialData(onvif_SerialData * p_data);
#endif // end of DEVICEIO_SUPPORT

/***************************************************************************************/

#ifdef THERMAL_SUPPORT
HT_API ColorPaletteList *                        onvif_add_ColorPalette(ColorPaletteList ** p_head);
HT_API void                                      onvif_free_ColorPalettes(ColorPaletteList ** p_head);

HT_API NUCTableList *                            onvif_add_NUCTable(NUCTableList ** p_head);
HT_API void                                      onvif_free_NUCTables(NUCTableList ** p_head);

HT_API BOOL                                      onvif_init_Thermal(VideoSourceList * p_req);
#endif // end of THERMAL_SUPPORT

/***************************************************************************************/

#ifdef CREDENTIAL_SUPPORT
HT_API CredentialList *                          onvif_add_Credential();
HT_API CredentialList *                          onvif_find_Credential(CredentialList * p_head, const char * token);
HT_API void                                      onvif_free_Credential(CredentialList ** p_head, CredentialList * p_node);
HT_API void                                      onvif_free_Credentials(CredentialList ** p_head);
HT_API BOOL                                      onvif_init_Credential();
#endif // end of CREDENTIAL_SUPPORT

/***************************************************************************************/

#ifdef ACCESS_RULES
HT_API AccessProfileList *                       onvif_add_AccessProfile();
HT_API AccessProfileList *                       onvif_find_AccessProfile(AccessProfileList * p_head, const char * token);
HT_API void                                      onvif_free_AccessProfile(AccessProfileList ** p_head, AccessProfileList * p_node);
HT_API void                                      onvif_free_AccessProfiles(AccessProfileList ** p_head);
HT_API BOOL                                      onvif_init_AccessProfile();
#endif // end of ACCESS_RULES

/***************************************************************************************/

#ifdef SCHEDULE_SUPPORT
HT_API ScheduleList *                            onvif_add_Schedule();
HT_API ScheduleList *                            onvif_find_Schedule(ScheduleList * p_head, const char * token);
HT_API void                                      onvif_free_Schedule(ScheduleList ** p_head, ScheduleList * p_node);
HT_API void                                      onvif_free_Schedules(ScheduleList ** p_head);
HT_API BOOL                                      onvif_init_Schedule();

HT_API SpecialDayGroupList *                     onvif_add_SpecialDayGroup();
HT_API SpecialDayGroupList *                     onvif_find_SpecialDayGroup(SpecialDayGroupList * p_head, const char * token);
HT_API void                                      onvif_free_SpecialDayGroup(SpecialDayGroupList ** p_head, SpecialDayGroupList * p_node);
HT_API void                                      onvif_free_SpecialDayGroups(SpecialDayGroupList ** p_head);
HT_API BOOL                                      onvif_init_SpecialDayGroup();
#endif // end of SCHEDULE_SUPPORT

/***************************************************************************************/

#ifdef RECEIVER_SUPPORT
HT_API ReceiverList *                            onvif_add_Receiver(ReceiverList ** p_head);
HT_API ReceiverList *                            onvif_find_Receiver(ReceiverList * p_head, const char * token);
HT_API void                                      onvif_free_Receiver(ReceiverList ** p_head, ReceiverList * p_node);
HT_API void                                      onvif_free_Receivers(ReceiverList ** p_head);
HT_API int                                       onvif_get_Receiver_nums(ReceiverList * p_head);
#endif // end of RECEIVER_SUPPORT

/***************************************************************************************/

#ifdef IPFILTER_SUPPORT
HT_API BOOL                                      onvif_is_ipaddr_filter_exist(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
HT_API onvif_PrefixedIPAddress *                 onvif_find_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
HT_API onvif_PrefixedIPAddress *                 onvif_get_idle_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size);
HT_API ONVIF_RET                                 onvif_add_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
#endif // IPFILTER_SUPPORT

/***************************************************************************************/
HT_API void                                      onvif_init_MulticastConfiguration(onvif_MulticastConfiguration * p_cfg);
HT_API void                                      onvif_init_VideoEncoder2ConfigurationOptions(onvif_VideoEncoder2ConfigurationOptions * p_option, const char * Encoding);
HT_API void                                      onvif_init_cfg();
HT_API void                                      onvif_init();



#ifdef __cplusplus
}
#endif

#endif


