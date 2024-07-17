#include <iostream>

#include "args.h"
#include "parser.h"
#include "conductor.h"
#include "common/utils.h"
#include "common/logging.h"
#include "recorder/recorder_manager.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    std::shared_ptr<Conductor> conductor = Conductor::Create(args);
    std::unique_ptr<RecorderManager> recorder_mgr;

    if (Utils::CreateFolder(args.record_path)) {
        recorder_mgr = RecorderManager::Create(conductor, args.record_path);
        DEBUG_PRINT("Recorder is running!");
    } else {
        DEBUG_PRINT("Recorder is not started!");
    }

    try {
        while (true) {
            conductor->CreatePeerConnection();

            DEBUG_PRINT("Wait for signaling!");
            conductor->AwaitCompletion();
        }
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
    
    return 0;
}
