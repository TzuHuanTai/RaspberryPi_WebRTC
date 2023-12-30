#include <iostream>

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

        conductor->Timeout(5);

        std::cout << "=> main: wait for closing!" << std::endl;
        conductor->streaming_state.wait(lock, [&]{return !conductor->IsReadyForStreaming();});
    }
    
    return 0;
}
