#ifndef DISPATCHER2_PACKET_H
#define DISPATCHER2_PACKET_H

//#include <memory>
//#include <cstdint>
//#include <cstring>

//struct Packet
//{
//    Packet():data(nullptr),len(0),ts(0),seq(0){}
//    Packet(uint8_t * pdata, int len, uint32_t ts, uint16_t seq)
//            :len(len),ts(ts),seq(seq)
//    {
//        data=std::make_unique<uint8_t[]>(len);
//        std::memcpy(data.get(),pdata,len);
//    }
//    std::unique_ptr<uint8_t []> data;
//    int len;
//    uint32_t ts;
//    uint16_t seq;
//};

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct _Packet
{
    uint8_t* data;
    int len;
    uint32_t ts;
    uint16_t seq;
}Packet;


static Packet* Packet_new(uint8_t* pdata, int len, uint32_t ts, uint16_t seq)
{
    Packet* p = (Packet*)(malloc(sizeof (Packet)));
    p->data=(uint8_t*)(malloc(sizeof(uint8_t)*len));
    memcpy(p->data,pdata,len);
    p->len=len;
    p->ts=ts;
    p->seq=seq;
    return p;
}

#endif //DISPATCHER2_PACKET_H
