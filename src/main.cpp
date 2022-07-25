#include <iostream>
#include <string>
#include <unistd.h>

#include "conductor.h"
#include "config.h"
#include "signal.h"

int main(int argc, char *argv[])
{
    SignalingConfig config;
    if(argc>1){
        config.signaling_url = argv[1];
    }

    std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>(config.signaling_url);

    auto signalr = new SignalServer(config.signaling_url, conductor);

    signalr->Connect();

    signalr->ClientJoin("6&2F27E633&0&0000");

    for (int i=0; i<10; i++){
        sleep(10);
        conductor->SendData("hihi! test send data");
    }

    sleep(600);

    signalr->Disconnect();
    std::cout << "=> main: end" << std::endl;

    return 0;
}
