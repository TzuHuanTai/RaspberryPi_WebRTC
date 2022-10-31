#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>
#include <ctime>

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

    while (true) {
        std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>(config.signaling_url);

        SignalServer signalr(config.signaling_url, conductor);
        signalr.JoinAsServer();
            
        std::cout << "=> main: SendData!" << std::endl;

        for (int i = 0; i < 5; i++)
        {
            sleep(5);
            conductor->SendData("hihi! test send data");
        }

        std::cout << "=> main: start loop!" << std::endl;
        
        auto start = std::chrono::system_clock::now();
        std::time_t end_time;
        while (conductor->loop) {
            sleep(1);
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end-start;
            end_time = std::chrono::system_clock::to_time_t(end);
            std::cout << '\r' << "elapsed time: " << elapsed_seconds.count() << "s" << "\e[K" << std::flush;
        }
        
        std::cout << std::ctime(&end_time);
    }
    
    exit(0);
    
    return 0;
}
