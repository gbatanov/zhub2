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

#ifndef	RTSP_RSUA_H
#define	RTSP_RSUA_H

#include "rtsp_parse.h"
#include "rtsp_media.h"

#ifdef RTSP_BACKCHANNEL
#include "audio_decoder.h"
#include "audio_play.h"
#include "rtp_rx.h"
#include "aac_rtp_rx.h"
#include "pcm_rtp_rx.h"
#endif

#ifdef RTSP_DEMO
#define MAX_NUM_RUA			4
#else
#   ifdef MEDIA_LIVE
#define MAX_NUM_RUA         10
#   else
#define MAX_NUM_RUA			100
#   endif
#endif

#define AV_TYPE_VIDEO       0
#define AV_TYPE_AUDIO       1
#define AV_TYPE_METADATA    2
#define AV_TYPE_BACKCHANNEL 3

#define AV_MAX_CHS          4

#define AV_VIDEO_CH         AV_TYPE_VIDEO
#define AV_AUDIO_CH         AV_TYPE_AUDIO
#define AV_METADATA_CH      AV_TYPE_METADATA
#define AV_BACK_CH          AV_TYPE_BACKCHANNEL


typedef enum rtsp_server_states
{
	RSS_NULL = 0,	                            // IDLE
	RSS_OPTIONS,	                            // OPTIONS Message has been received
	RSS_DESCRIBE,	                            // DESCRIBE request is received
	RSS_ANNOUNCE,	                            // ANNOUNCE request is received
	RSS_INIT_V,		                            // Initial state: video channel SETUP request is received
	RSS_INIT_A,		                            // Initial state: Audio channel SETUP request is received
	RSS_READY,		                            // Ready state: SETUP reply received or PAUSE reply received while in playback mode
	RSS_PLAYING,	                            // Play state: Received PLAY, and already replied, is being sent RTP
	RSS_PAUSE,                                  // Pause state: Received PAUSE and already replied
	RSS_RECORDING,	                            // Record state: receive RECORD, and has been replied
} RSSTATE;

typedef struct ua_replay_header
{
	uint16			magic;						// 0xABAC
	uint16			length;						// 3
	uint32			ntp_sec;
	uint32			ntp_frac;

	uint32			mbz : 4;					// This field is reserved for future use and must be zero
	uint32			t : 1;						// Indicates that this is the terminal frame on playback of a track. 
												//	A device should signal this flag in both forward and reverse playback whenever no more data is available for a track
	uint32			d : 1;						// Indicates that this access unit follows a discontinuity in transmission.
												//	It is primarily used during reverse replay; 
												//	the first packet of each GOP has the D bit set since it does not chronologically follow the previous packet in the data stream 
	uint32			e : 1;						// Indicates the end of a contiguous section of recording. 
												//	The last access unit in each track before a recording gap, or at the end of available footage, shall have this bit set. 
												// 	When replaying in reverse, the E flag shall be set on the last frame at the end of the contiguous section of recording
	uint32			c : 1;						// Indicates that this access unit is a synchronization point or clean point, 
												//	e.g. the start of an intra-coded frame in the case of video streams
	uint32			seq : 8;					// This is the low-order byte of the Cseq value used in the RTSP PLAY command that was used to initiate transmission. 
												//	When a client sends multiple, consecutive PLAY commands, this value may be used to determine where the data from each new PLAY command begins
	uint32			padding : 16;
} UA_REPLAY_HDR;

typedef struct rtsp_media_channel
{
    SOCKET          udp_fd;                     // udp socket
    char			ctl[64];                    // control string
    uint16          setup;                      // whether the media channel already be setuped
    uint16	        r_port;                     // remote udp port
	uint16	        l_port;                     // local udp port
	uint16	        interleaved;	            // rtp channel values
    char            destination[32];            // multicast address
    int				cap_count;                  // Local number of capabilities
	uint8	        cap[MAX_AVN];               // Local capability
	char			cap_desc[MAX_AVN][MAX_AVDESCLEN];
	UA_RTP_INFO		rtp_info;                   // rtp info

#ifdef RTSP_RTCP
    SOCKET          rtcp_fd;                    // rtcp udp socket
    UA_RTCP_INFO    rtcp_info;                  // rtcp info
#endif

#ifdef RTSP_REPLAY
    UA_REPLAY_HDR	rep_hdr;                    // replay header
#endif
} RSMCH;

typedef struct rtsp_server_ua
{
	uint32	        used_flag	: 1;	        // used flag
	uint32	        rtp_tcp		: 1;	        // whether to use RTP OVER RTSP mode transmission
	uint32	        rtp_unicast	: 1;	        // whether RTP unicast mode
	uint32	        rtp_tx		: 1;	        // RTP thread starts, is sending RTP
	uint32			rtp_rx 		: 1;			// RTP receive flag
	uint32          rtcp_rx     : 1;            // RTCP receive flag
	uint32	        rtp_pause   : 1;	        // RTP thread pause send data
	uint32	        iframe_tx	: 1;            // if i-frame already sended
	uint32	        play_range	: 1;            // whether the play range is valid
	uint32          skip_rtcp   : 1;            // Don't send RTCP sender reports
	uint32          backchannel : 1;            // audio back channel flag
	uint32          ad_inited   : 1;            // audio decoder init flag
	uint32			replay		: 1;			// replay flag
	uint32          mc_ref      : 1;            // multicast reference
	uint32          mc_src      : 1;            // multicast source
	uint32          mc_del      : 1;            // multicast delete flag
	uint32	        reserved	: 16;
	
	RSSTATE			state;                      // server state
	SOCKET			fd;                         // tcp socket
    void          * fd_mutex;                   // fd write mutex
	uint32	        cseq;                       // seq no.
	time_t          lats_rx_time;               // last received packet time
	char			sid[64];		            // session id
	char			uri[256];                   // the requested uri
	char			cbase[256];		            // Content-Base: rtsp://221.10.50.195:554/broadcast.sdp/

	uint32	        user_real_ip;	            // user real ip address (network byte order)
	uint16	        user_real_port; 	        // user real port (host byte order)

	char			rcv_buf[2052];	            // Receive Buffer
	int				rcv_dlen;		            // Already exists in the data buffer length
    int				rtp_t_len;                  // rtp payload total lenght
	int				rtp_rcv_len;                // rtp payload current receive length
	char          *	rtp_rcv_buf;                // rtp payload receive buffer

	int				play_range_type;			// 0 - Relative Time, 1 - Absolute time
    int64           play_range_begin;           // play range begin
    int64           play_range_end;             // play range end

    RSMCH           channels[AV_MAX_CHS];       // media channels

	pthread_t		rtp_thread;			        // Send RTP thread ID
    pthread_t       audio_thread;               // audio capture thread ID
    
	UA_MEDIA_INFO   media_info;                 // media information

    HD_AUTH_INFO    auth_info;                  // auth information
    
#ifdef RTSP_BACKCHANNEL
    int             bc_codec;                   // back channel audio codec
    int             bc_samplerate;              // back channel sample rate
    int             bc_channels;                // back channel channel nums

    union {
        AACRXI      aacrxi;                     // back channel data receiving
	    PCMRXI      pcmrxi;                     // back channel data receiving
	};

    pthread_t       tid_udp_rx;                 // udp data receiving thread

#ifdef MEDIA_LIVE
#else
	CAudioDecoder * audio_decoder;              // audio decoder
	CAudioPlay    * audio_player;               // audio player
#endif	
#endif // end of RTSP_BACKCHANNEL

#ifdef RTSP_OVER_HTTP
    char            sessioncookie[100];         // rtsp over http session cookie
    void          * rtsp_send;                  // rtsp over http get connection
    void          * rtsp_recv;                  // rtsp over http post connection
#endif

#ifdef RTSP_REPLAY
    time_t          s_replay_time;              // replay start time
	int			    scale;						// scale info, 100 means 1.0, Divide by 100 when using
	int				rate_control;				// rate control flag, 
												//	1-the stream is delivered in real time using standard RTP timing mechanisms
												//  0-the stream is delivered as fast as possible, using only the flow control provided by the transport to limit the delivery rate
	int				immediate;					// 1 - immediately start playing from the new location, cancelling any existing PLAY command.
												//	The first packet sent from the new location shall have the D (discontinuity) bit set in its RTP extension header. 
	int				frame;						// 0 - all frames
												// 1 - I-frame and P-frame
												// 2 - I-frame
	int				frame_interval;				// I-frame interval, unit is milliseconds
#endif

#ifdef RTSP_RTCP
    pthread_t       tid_rtcp_rx;                // rtcp udp data receiving thread
#endif
}RSUA;

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/
HRTSP_MSG * rua_build_security_response(RSUA * p_rua);
HRTSP_MSG * rua_build_options_response(RSUA * p_rua);
HRTSP_MSG * rua_build_get_parameter_response(RSUA * p_rua);
HRTSP_MSG * rua_build_descibe_response(RSUA * p_rua);
BOOL        rua_build_sdp_msg(RSUA * p_rua, HRTSP_MSG * tx_msg);
int         rtsp_cacl_sdp_length(HRTSP_MSG * tx_msg);

HRTSP_MSG * rua_build_setup_response(RSUA * p_rua, int av_t);
HRTSP_MSG * rua_build_play_response(RSUA * p_rua);

HRTSP_MSG * rua_build_response(RSUA * p_rua, const char * resp_str);
/*************************************************************************/
BOOL        rua_get_transport_info(RSUA * p_rua, char * transport_buf, int av_t);
BOOL        rua_get_play_range_info(RSUA * p_rua, char * range_buf);

#ifdef MEDIA_PUSHER
BOOL        rsua_get_sdp_h264_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rsua_get_sdp_h265_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rsua_get_sdp_mp4_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rsua_get_sdp_mp4_params(RSUA * p_rua, int * pt, char * p_cfg, int max_len);
BOOL        rsua_get_sdp_aac_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len);
BOOL        rsua_get_sdp_aac_params(RSUA * p_rua, int *pt, int *sizelength, int *indexlength, int *indexdeltalength, char * p_cfg, int max_len);
#endif

/*************************************************************************/
void        rsua_send_rtsp_msg(RSUA * p_rua,HRTSP_MSG * tx_msg);

#define     rsua_send_free_rtsp_msg(p_rua,tx_msg) \
                do { \
                    rsua_send_rtsp_msg(p_rua,tx_msg); \
                    rtsp_free_msg(tx_msg); \
                } while(0)

/*************************************************************************/
void        rua_proxy_init();
void        rua_proxy_deinit();

RSUA *      rua_get_idle_rua();
void        rua_set_idle_rua(RSUA * p_rua);
void        rua_set_online_rua(RSUA * p_rua);

/*************************************************************************/
RSUA *      rua_lookup_start();
RSUA *      rua_lookup_next(RSUA * p_rua);
void        rua_lookup_stop();

/*************************************************************************/
uint32      rua_get_index(RSUA * p_rua);
RSUA *      rua_get_by_index(uint32 index);

#ifdef RTSP_OVER_HTTP
RSUA *      rua_get_by_sessioncookie(char * sessioncookie);
#endif

/*************************************************************************/
BOOL        rsua_init_udp_connection(RSUA * p_rua, int av_t, uint32 lip);
BOOL        rsua_init_mc_connection(RSUA * p_rua, int av_t, uint32 lip);

#ifdef RTSP_RTCP
BOOL        rsua_init_rtcp_udp_connection(RSUA * p_rua, int av_t, uint32 lip);
BOOL        rsua_init_rtcp_mc_connection(RSUA * p_rua, int av_t, uint32 lip);
#endif


#ifdef __cplusplus
}
#endif

#endif // RTSP_RSUA_H



