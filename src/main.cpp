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

    std::shared_ptr<Conductor> conductor = std::make_shared<Conductor>();

    while (true) {
        conductor->CreatePeerConnection();

        std::cout << "=> main: start signalr!" << std::endl;
        SignalServer signalr(config.signaling_url, conductor);
        signalr.JoinAsServer();

        std::cout << "=> main: wait for ready streaming!" << std::endl;
        std::unique_lock<std::mutex> lock(conductor->mtx);
        conductor->cond_var.wait(lock, [&]{return conductor->is_ready_for_streaming;});

        auto start = std::chrono::system_clock::now();
        std::time_t end_time;
        while (conductor->is_ready_for_streaming) {
            sleep(1);
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = end-start;
            end_time = std::chrono::system_clock::to_time_t(end);
            std::cout << '\r' << "elapsed time: " << elapsed_seconds.count() << "s" << "\e[K" << std::flush;
        }
        
        std::cout << std::ctime(&end_time);

        sleep(1);
    }
    
    return 0;
}
