#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>
#include <ctime>

#include "args.h"
#include "conductor.h"
#include "parser.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    auto conductor = Conductor::Create(args);

    while (true) {
        conductor->CreatePeerConnection();

        std::cout << "=> main: wait for signaling!" << std::endl;
        std::unique_lock<std::mutex> lock(conductor->state_mtx);
        conductor->streaming_state.wait(lock, [&]{return conductor->IsReadyForStreaming();});

        auto f = std::async(std::launch::async, [&]() {
            sleep(20);
            if (conductor->IsReadyForStreaming() && !conductor->IsConnected()) {
                conductor->SetStreamingState(false);
            }
        });

        std::cout << "=> main: wait for closing!" << std::endl;
        conductor->streaming_state.wait(lock, [&]{return !conductor->IsReadyForStreaming();});
    }
    
    return 0;
}
