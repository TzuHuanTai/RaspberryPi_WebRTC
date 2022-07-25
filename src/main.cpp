#include <iostream>
#include <string>
#include <unistd.h>

#include "conductor.h"
#include "config.h"
#include "signal.h"

int main(int argc, char *argv[])
{
    SignalingConfig config;
    if (argc > 1)
    {
        config.signaling_url = argv[1];
    }

    std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>(config.signaling_url);

    SignalServer signalr(config.signaling_url, conductor);
    signalr.ListenOfferSdp()
        .ListenOfferIce()
        .SetAnswerSdp()
        .SetAnswerIce()
        .SetDisconnect()
        .Connect()
        .JoinAsClient("6&2F27E633&0&0000");

    for (int i = 0; i < 5; i++)
    {
        sleep(10);
        conductor->SendData("hihi! test send data");
    }

    while (conductor->loop) {
        sleep(1);
    }

    return 0;
}
