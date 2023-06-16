#ifndef CDISPATCHER_H
#define CDISPATCHER_H

#include <thread>
#include <memory>

#include <iostream>

//BEGIN Happytime
#include "sys_inc.h"
#include "rtsp_cln.h"
#include "hqueue.h"     //queue
//END Happytime

#include "packet.h"     //Packet

extern HQUEUE packetQueue;

struct Event{
    Event(int e=0, CRtspClient* r= nullptr):event(e),rtsp(r){}
    int 		  event;
    CRtspClient * rtsp;
};

class CDispatcher{
public:
    CDispatcher(std::string urlSrc, std::string username="", std::string pwd="");
    ~CDispatcher();

    void reconnect();

    inline HQUEUE& eventQueue(){return *m_eventQueue;}
    CRtspClient& rtspClient();
private:
    void connect();

    CRtspClient m_rtspClient;

    std::unique_ptr<HQUEUE> m_eventQueue;

    std::thread m_notifyThread;

    bool m_connection=false;

    std::string m_urlSrc;
    std::string m_username;
    std::string m_password;
};

int rtspVideoCB(uint8 * pdata, int len, uint32 ts, uint16 seq, void * puser);
int rtspNotifyCB(int event, void * puser);
void rtspNotifyHandler(void * argc);

#endif // CDISPATCHER_H
