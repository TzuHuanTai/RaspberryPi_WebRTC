#include "signal.h"

#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <unistd.h>

SignalServer::SignalServer(std::string url, std::shared_ptr<Conductor> conductor)
    : url(url),
      conductor_(conductor),
      connection_(signalr::hub_connection_builder::create(url).build())
{
    // conductor_->answer_sdp_ = [this](std::string sdp)
    // {
    //     this->SendMessage("AnswerSDP", sdp.c_str());
    // };

    // conductor_->answer_ice_ = [this](std::string candidate)
    // {
    //     this->SendMessage("AnswerICE", candidate.c_str());
    // };

    // this->Subscribe(
    //     "OfferSPD",
    //     [this](const std::vector<signalr::value> &m)
    //     {
    //         const std::string sdp = m[0].as_string();
    //         std::cout << "Receive OfferSPD: " << sdp << std::endl;
    //         conductor_->SetOfferSDP(sdp, [this](){
    //             conductor_->CreateAnswer([this](webrtc::SessionDescriptionInterface* desc){
    //                 std::string sdp;
    //                 desc->ToString(&sdp);
    //                 this->SendMessage("AnswerSDP", sdp.c_str());
    //             }, nullptr);
    //         }, nullptr);
    //     });

    Subscribe(
        "OfferICE",
        [](const std::vector<signalr::value> &m)
        {
            std::cout << "Receive OfferICE: " << m[0].as_string() << std::endl;
        });

    // conductor_->ConnectToPeer();
}

void SignalServer::Connect()
{
    std::promise<void> start_task;
    connection_.start([&start_task](std::exception_ptr exception)
                      { start_task.set_value(); });
    start_task.get_future().get();
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

void SignalServer::SendMessage(std::string method, const char *context)
{
    std::promise<void> send_task;
    std::vector<signalr::value> args{std::string("c++"), context};
    connection_.invoke(method, args,
                       [&send_task](const signalr::value &value, std::exception_ptr exception)
                       {
                           send_task.set_value();
                       });
    send_task.get_future().get();
};

void SignalServer::SendTest()
{
    std::promise<void> send_task;
    std::vector<signalr::value> args{std::string("c++"), "Client", std::string("c++"), "webrtc64b32g"};
    connection_.invoke("ClientJoin", args,
                       [&send_task](const signalr::value &value, std::exception_ptr exception)
                       {
                           send_task.set_value();
                       });
    send_task.get_future().get();
};

void SignalServer::SendTest2()
{
    std::promise<void> send_task;
    std::vector<signalr::value> args{ std::string("c++"), "Client" };
    connection_.invoke("Echo", args,
                       [&send_task](const signalr::value &value, std::exception_ptr exception)
                       {
                           send_task.set_value();
                       });
    send_task.get_future().get();
};