#ifndef SIGNAL_H_
#define SIGNAL_H_

#include "conductor.h"

#include <signalrclient/hub_connection.h>
#include <signalrclient/hub_connection_builder.h>
#include <signalrclient/signalr_value.h>


class SignalServer : public std::enable_shared_from_this<SignalServer>
{
public:
    std::string url;
    SignalServer(std::string url, std::shared_ptr<Conductor> conductor);
    ~SignalServer(){};

    void Connect();
    void Disconnect();
    void Subscribe(std::string event_name, const signalr::hub_connection::method_invoked_handler& handler);
    void SendMessage(std::string method, const char *context);
    void SendTest();
    void SendTest2();

private:
    std::shared_ptr<Conductor> conductor_;
    signalr::hub_connection connection_;
};

#endif