#include <iostream>
#include <string>
#include <unistd.h>
#include <chrono>
#include <ctime>

#include "args.h"
#include "conductor.h"
#include "parser.h"
#include "signaling/signaling_service.h"
#if USE_WEBSOCKET_SIGNALING
#endif
#if USE_SIGNALR_SIGNALING
#include "signaling/signalr_server.h"
#endif

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    auto conductor = Conductor::Create(args);

    while (true) {
        conductor->CreatePeerConnection();

        auto signal = ([&]() -> std::unique_ptr<SignalingService> {
        #if USE_WEBSOCKET_SIGNALING
            return nullptr;
        #elif USE_SIGNALR_SIGNALING
            return SignalrService::Create(args.signaling_url, conductor);
        #else
            return nullptr;
        #endif
        })();

        std::cout << "=> main: wait for signaling!" << std::endl;
        std::unique_lock<std::mutex> lock(conductor->mtx);
        conductor->cond_var.wait(lock, [&]{return conductor->is_ready_for_streaming;});

        sleep(1);
        conductor->cond_var.wait(lock, [&]{return !conductor->is_ready_for_streaming;});
    }
    
    return 0;
}
