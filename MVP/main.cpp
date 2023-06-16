#include <memory>

extern "C"{
#include "videocorehtconnector.h"
}

#include "cdispatcher.h"

int main(int argc, char** argv){
    if(argc!=2) {
        std::cout<<"Using: "<<argv[0]<<" rtsp://ip:port/stream"<<std::endl;
        return EXIT_FAILURE;
    }

    auto dispatcher=std::make_unique<CDispatcher>(argv[1]);

    GstVideocoreHtConnector* vcc;
    vcc = gst_videocore_ht_connector_new();

    while(1);

    return EXIT_SUCCESS;
}
