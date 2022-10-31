#ifndef SIGNAL_H_
#define SIGNAL_H_

#include "conductor.h"

#include <mutex>
#include <condition_variable>

#include <signalrclient/hub_connection.h>
#include <signalrclient/hub_connection_builder.h>
#include <signalrclient/signalr_value.h>

struct SignalingTopic
{
    std::string offer_sdp = "OfferSDP";
    std::string offer_ice = "OfferICE";
    std::string answer_sdp = "AnswerSDP";
    std::string answer_ice = "AnswerICE";
    std::string connected_client = "ConnectedClient";
    std::string join_as_client = "JoinAsClient";
    std::string join_as_server = "JoinAsServer";
};

class SignalServer : public std::enable_shared_from_this<SignalServer>
{
public:
    std::string url;
    SignalingTopic topics;
    std::mutex mtx;
    std::condition_variable cond_var;
    bool ready = false;

    SignalServer(std::string url, std::shared_ptr<Conductor> conductor);
    ~SignalServer(){};

    SignalServer &Connect();
    SignalServer &ListenClientId();
    SignalServer &ListenOfferSdp();
    SignalServer &ListenOfferIce();
    SignalServer &SetAnswerSdp();
    SignalServer &SetAnswerIce();
    SignalServer &SetDisconnect();
    void Disconnect();
    void Subscribe(std::string event_name, const signalr::hub_connection::method_invoked_handler &handler);
    void JoinAsClient();
    void JoinAsServer();

private:
    std::string client_id_;
    void SendMessage(std::string method, std::vector<signalr::value> args);
    std::shared_ptr<Conductor> conductor_;
    signalr::hub_connection connection_;
};

#endif