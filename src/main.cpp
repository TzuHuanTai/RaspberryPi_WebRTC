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

        std::cout << "[main] wait for signaling!" << std::endl;
        conductor->BlockUntilSignal();

        std::cout << "[main] timeout waiting..." << std::endl;
        conductor->BlockUntilCompletion(5);
    }
    
    return 0;
}
