#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <string>

#include "signalrclient/hub_connection.h"
#include "signalrclient/hub_connection_builder.h"

class SignalServer {
   public:
    SignalServer(std::string server_url);
    ~SignalServer();

    void Start();
    void Send(std::string method, const char* context);

   private:
    std::unique_ptr<signalr::hub_connection> connection_;
};

#endif