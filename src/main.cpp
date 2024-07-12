#include <iostream>

#include "args.h"
#include "parser.h"
#include "conductor.h"
#include "common/utils.h"
#include "common/logging.h"
#include "recorder/background_recorder.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    std::shared_ptr<Conductor> conductor = Conductor::Create(args);
    std::unique_ptr<BackgroundRecorder> bg_recorder;

    if (Utils::CreateFolder(args.record_path)) {
        bg_recorder = BackgroundRecorder::Create(conductor);
        bg_recorder->Start();
        DEBUG_PRINT("Background recorder is running!");
    } else {
        DEBUG_PRINT("Background recorder is not started!");
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
