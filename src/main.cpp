#include <iostream>

#include "args.h"
#include "parser.h"
#include "conductor.h"
#include "common/utils.h"
#include "recorder/background_recorder.h"

int main(int argc, char *argv[]) {
    Args args;
    Parser::ParseArgs(argc, argv, args);

    std::shared_ptr<Conductor> conductor = Conductor::Create(args);
    std::unique_ptr<BackgroundRecorder> bg_recorder;

    if (Utils::CreateFolder(args.record_path)) {
        bg_recorder = BackgroundRecorder::Create(conductor);
        bg_recorder->Start();
        std::cout << "[main] recorder is created!" << std::endl;
    } else {
        std::cout << "[main] recorder is not started!" << std::endl;
    }

    try {
        while (true) {
            conductor->CreatePeerConnection();

            std::cout << "[main] wait for signaling!" << std::endl;
            conductor->AwaitCompletion();
        }
    } catch (const std::exception &e) {
        std::cerr << "Error in [main]: " << e.what() << std::endl;
    }
    
    return 0;
}
