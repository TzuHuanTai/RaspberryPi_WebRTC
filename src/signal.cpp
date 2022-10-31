#include "signal.h"

#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <map>
#include <unistd.h>

SignalServer::SignalServer(std::string url, std::shared_ptr<Conductor> conductor)
    : url(url),
      conductor_(conductor),
      connection_(signalr::hub_connection_builder::create(url).build())
{
}

SignalServer &SignalServer::ListenClientId()
{
    Subscribe(
        topics.connected_client,
        [this](const std::vector<signalr::value> &m)
        {
            std::cout << "[SignalR] ConnectedClient: " << std::endl;
            std::unique_lock<std::mutex> lock(mtx);
            if(client_id_.empty()){
                client_id_ = m[0].as_string();
                std::cout << client_id_ << std::endl;
            } else {
                std::cout << m[0].as_string() << " want to replace " << client_id_ << std::endl;
            }

            ready = true;
            cond_var.notify_all();
        });
    return *this;
}

SignalServer &SignalServer::ListenOfferSdp()
{
    Subscribe(
        topics.offer_sdp,
        [this](const std::vector<signalr::value> &m)
        {
            std::cout << "[SignalR] OfferSDP: Received!" << std::endl;
            const std::map<std::string, signalr::value> object = m[0].as_map();

            std::string sdp;
            for (const auto &s : object)
            {
                std::cout << "=> key: " << s.first << ", value: " << s.second.as_string() << std::endl;
                if (s.first == "sdp")
                {
                    sdp = s.second.as_string();
                }
            }

            conductor_->SetOfferSDP(
                sdp,
                [this]()
                {
                    conductor_->CreateAnswer(
                        [this](webrtc::SessionDescriptionInterface *desc)
                        {
                            std::string answer_sdp;
                            desc->ToString(&answer_sdp);
                            conductor_->invoke_answer_sdp(answer_sdp);
                        },
                        nullptr);
                },
                nullptr);
        });
    return *this;
}

SignalServer &SignalServer::ListenOfferIce()
{
    Subscribe(
        topics.offer_ice,
        [this](const std::vector<signalr::value> &m)
        {
            std::cout << "=> OfferICE: Received!" << std::endl;
            const std::map<std::string, signalr::value> ice = m[0].as_map();

            std::string sdp_mid;
            int sdp_mline_index;
            std::string candidate;

            for (const auto &s : ice)
            {
                if (s.first == "candidate")
                {
                    candidate = s.second.as_string();
                }
                else if (s.first == "sdpMLineIndex")
                {
                    sdp_mline_index = (int)s.second.as_double();
                }
                else if (s.first == "sdpMid")
                {
                    sdp_mid = s.second.as_string();
                }
            }

            std::cout << "=> OfferICE: " << sdp_mline_index << ", " << sdp_mid << ", " << candidate << std::endl;

            // bug: Failed to apply the received candidate. connect but without datachannel!?
            // conductor_->AddIceCandidate(sdp_mid, sdp_mline_index, candidate);
        });
    return *this;
}

SignalServer &SignalServer::SetAnswerSdp()
{
    conductor_->invoke_answer_sdp = [this](std::string sdp)
    {
        std::unique_lock<std::mutex> lock(mtx);
        std::cout << "[SignalR] Invoke AnswerSDP: " << sdp << std::endl;
        std::map<std::string, signalr::value> sdp_message = {
            {"sdp", sdp},
            {"type", "answer"}};

        std::cout << "[SignalR] Invoke AnswerSDP: wait!" << std::endl;
        cond_var.wait(lock, [this]{return ready;});
        std::cout << "[SignalR] Invoke AnswerSDP: got clientId " << client_id_ << std::endl;
        
        std::vector<signalr::value> args{client_id_, sdp_message};
        SendMessage(topics.answer_sdp, args);
    };
    return *this;
}

SignalServer &SignalServer::SetAnswerIce()
{
    conductor_->invoke_answer_ice = [this](std::string sdp_mid, int sdp_mline_index, std::string candidate)
    {
        std::unique_lock<std::mutex> lock(mtx);
        std::cout << "[SignalR] Invoke AnswerICE: " << sdp_mid << ", " << sdp_mline_index << ", " << candidate << std::endl;
        std::map<std::string, signalr::value> ice_message = {
            {"sdpMid", sdp_mid},
            {"sdpMLineIndex", static_cast<double>(sdp_mline_index)},
            {"candidate", candidate}};

        std::cout << "[SignalR] Invoke AnswerICE: wait!" << std::endl;
        cond_var.wait(lock, [this]{return ready;});
        std::cout << "[SignalR] Invoke AnswerICE: got clientId " << client_id_ << std::endl;
        
        std::vector<signalr::value> args{client_id_, ice_message};
        SendMessage(topics.answer_ice, args);
    };
    return *this;
}

SignalServer &SignalServer::SetDisconnect()
{
    conductor_->complete_signaling = [this](){
        sleep(3);
        Disconnect();
    };
    return *this;
}

SignalServer &SignalServer::Connect()
{
    std::promise<void> start_task;
    connection_.start([&start_task](std::exception_ptr exception)
                      { start_task.set_value(); });
    start_task.get_future().get();
    return *this;
};

void SignalServer::Disconnect()
{
    std::promise<void> stop_task;
    connection_.stop([&stop_task](std::exception_ptr exception)
                     { stop_task.set_value(); });
    stop_task.get_future().get();
};

void SignalServer::Subscribe(std::string event_name, const signalr::hub_connection::method_invoked_handler &handler)
{
    connection_.on(event_name, handler);
};

void SignalServer::SendMessage(std::string method, std::vector<signalr::value> args)
{
    std::promise<void> send_task;
    connection_.invoke(method, args,
                       [&send_task](const signalr::value &value, std::exception_ptr exception)
                       {
                           send_task.set_value();
                       });
    send_task.get_future().get();
};

void SignalServer::JoinAsClient()
{
    std::vector<signalr::value> args{};
    SendMessage(topics.join_as_client, args);
};

void SignalServer::JoinAsServer()
{
    std::cout << "=> SignalServer: start setting!" << std::endl;
    ListenClientId();
    ListenOfferSdp();
    ListenOfferIce();
    SetAnswerSdp();
    SetAnswerIce();
    SetDisconnect();

    Connect();

    std::vector<signalr::value> args{};
    SendMessage(topics.join_as_server , args);
};
