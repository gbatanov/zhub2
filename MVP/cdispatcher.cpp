#include "cdispatcher.h"

HQUEUE packetQueue;

CDispatcher::CDispatcher(std::string urlSrc, std::string username, std::string pwd)
    :m_urlSrc(urlSrc),m_username(username),m_password(pwd), m_eventQueue(std::make_unique<HQUEUE>()){

    network_init();

    packetQueue = *hqCreate(10, sizeof(Packet), HQ_GET_WAIT | HQ_PUT_WAIT);
    m_eventQueue.reset(hqCreate(10,sizeof(Event), HQ_GET_WAIT | HQ_PUT_WAIT));

    sys_buf_init(32);
    rtsp_parse_buf_init(100);

    m_notifyThread=std::thread(rtspNotifyHandler, this);
    connect();
}

CDispatcher::~CDispatcher() {
    m_rtspClient.rtsp_close();

    m_notifyThread.join();

    hqDelete(&*m_eventQueue);
    hqDelete(&packetQueue);

    rtsp_parse_buf_deinit();
    sys_buf_deinit();
}

void CDispatcher::connect() {
    m_rtspClient.set_notify_cb(rtspNotifyCB, this);
    m_rtspClient.set_video_cb(rtspVideoCB);

    m_connection=m_rtspClient.rtsp_start(m_urlSrc.c_str(),const_cast<char*>(m_username.c_str()),const_cast<char *>(m_password.c_str()));
}

void CDispatcher::reconnect() {
    m_connection= false;
    m_rtspClient.rtsp_close();
    connect();
}

CRtspClient &CDispatcher::rtspClient() {
    return m_rtspClient;
}

void  rtspNotifyHandler(void * argc){
    CDispatcher* d= static_cast<CDispatcher*>(argc);

    Event e;
    if(hqBufGet(&d->eventQueue(),(char*)&e)){
        if(RTSP_EVE_STOPPED == e.event || RTSP_EVE_CONNFAIL == e.event){
            d->reconnect();
        }
    }
    else{
        std::cerr<<__PRETTY_FUNCTION__<<" Error getting element from event queue"<<std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int rtspVideoCB(uint8 * pdata, int len, uint32 ts, uint16 seq, void * puser){
    hqBufPut(&packetQueue,(char*)Packet_new(pdata,len,ts,seq));
    printf("%s, len = %d, ts = %u, seq = %d\r\n"
           "packet queue size: %u\r\n", __FUNCTION__, len, ts, seq, hqSize(&packetQueue));
    return 0;
}

int rtspNotifyCB(int event, void * puser){
    auto dispatcher = static_cast<CDispatcher*>(puser);
    Event e(event,&dispatcher->rtspClient());
    if(!hqBufPut(&dispatcher->eventQueue(),(char*)&(e)))
    {
        std::cout<<__PRETTY_FUNCTION__<<" Error pushing element to the event queue"<<std::endl;
        return FALSE;
    }
    return TRUE;
}
