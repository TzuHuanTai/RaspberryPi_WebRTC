#include <iostream>
#include <string>
#include <unistd.h>

#include "conductor.h"
#include "config.h"
#include "signal.h"

int main(int argc, char *argv[])
{
    SignalingConfig config;

    std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>(config.signaling_url);

    auto signalr = new SignalServer(config.signaling_url, conductor);
    signalr->Subscribe(
        "OfferSPD", 
        [](const std::vector<signalr::value> &m)
        {
            const std::string sdp = m[0].as_string();
           std::cout << "Receive OfferSPD: " << sdp << std::endl;
        });

    signalr->Connect();

    //signalr->SendMessage("ClientJoin", "Client");
    signalr->SendTest();
    signalr->SendTest2();

    sleep(15);

    // conductor->ConnectToPeer();

    signalr->Disconnect();
    std::cout << "=> main: end" << std::endl;

    return 0;
}
