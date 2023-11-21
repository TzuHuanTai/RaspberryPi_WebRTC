#include "signaling/signalr_server.h"

#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <map>
#include <unistd.h>

std::unique_ptr<SignalrService> SignalrService::Create(std::string url,
                                    std::shared_ptr<Conductor> conductor) {
    auto ptr = std::make_unique<SignalrService>(url, conductor);
    ptr->AutoReconnect()
        .DisconnectOnCompleted()
        .Connect();
    return ptr;
}

SignalrService::SignalrService(std::string url, std::shared_ptr<Conductor> conductor)
    : SignalingService(conductor),
      url(url),
      connection_(signalr::hub_connection_builder::create(url).build()) {}

void SignalrService::ListenClientId() {
    Subscribe(topics.connected_client, [this](const std::vector<signalr::value> &m) {
        std::cout << "[SignalR] ConnectedClient: " << std::endl;
        std::unique_lock<std::mutex> lock(mtx);
        if (client_id_.empty()) {
            client_id_ = m[0].as_string();
            std::cout << client_id_ << std::endl;
        } else {
            std::cout << m[0].as_string() << " want to replace " << client_id_ << std::endl;
        }

        ready = true;
        cond_var.notify_all();
    });
}

void SignalrService::ListenOfferSdp() {
    Subscribe(topics.offer_sdp, [this](const std::vector<signalr::value> &m) {
        std::cout << "[SignalR] OfferSDP: Received!" << std::endl;
        const std::map<std::string, signalr::value> object = m[0].as_map();

        std::string sdp;
        for (const auto &s : object) {
            std::cout << "=> key: " << s.first << ", value: " << s.second.as_string() << std::endl;
            if (s.first == "sdp") {
                sdp = s.second.as_string();
            }
        }
        OnRemoteSdp(sdp);
    });
}

void SignalrService::ListenOfferIce() {
    Subscribe(topics.offer_ice, [this](const std::vector<signalr::value> &m) {
        std::cout << "=> OfferICE: Received!" << std::endl;
        const std::map<std::string, signalr::value> ice = m[0].as_map();

        std::string sdp_mid;
        int sdp_mline_index;
        std::string candidate;

        for (const auto &s : ice) {
            if (s.first == "candidate") {
                candidate = s.second.as_string();
            } else if (s.first == "sdpMLineIndex") {
                sdp_mline_index = (int)s.second.as_double();
            } else if (s.first == "sdpMid") {
                sdp_mid = s.second.as_string();
            }
        }
        std::cout << "=> OfferICE: " << sdp_mline_index << ", " << sdp_mid << ", " << candidate << std::endl;

        OnRemoteIce(sdp_mid, sdp_mline_index, candidate);
    });
}

void SignalrService::AnswerLocalSdp(std::string sdp) {
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "[SignalR] Invoke AnswerSDP: " << sdp << std::endl;
    std::map<std::string, signalr::value> sdp_message = {
            {"sdp", sdp},
            {"type", "answer"}};

    std::cout << "[SignalR] Invoke AnswerSDP: wait!" << std::endl;
    cond_var.wait(lock, [this]{ return ready; });
    std::cout << "[SignalR] Invoke AnswerSDP: got clientId " << client_id_ << std::endl;
        
    std::vector<signalr::value> args{client_id_, sdp_message};
    SendMessage(topics.answer_sdp, args);
}

void SignalrService::AnswerLocalIce(std::string sdp_mid, int sdp_mline_index,
                                  std::string candidate) {
    std::unique_lock<std::mutex> lock(mtx);
    std::cout << "[SignalR] Invoke AnswerICE: " << sdp_mid << ", " << sdp_mline_index << ", " << candidate << std::endl;
    std::map<std::string, signalr::value> ice_message = {
            {"sdpMid", sdp_mid},
            {"sdpMLineIndex", static_cast<double>(sdp_mline_index)},
            {"candidate", candidate}};

    std::cout << "[SignalR] Invoke AnswerICE: wait!" << std::endl;
    cond_var.wait(lock, [this]{ return ready; });
    std::cout << "[SignalR] Invoke AnswerICE: got clientId " << client_id_ << std::endl;
        
    std::vector<signalr::value> args{client_id_, ice_message};
    SendMessage(topics.answer_ice, args);
}

SignalrService &SignalrService::DisconnectOnCompleted() {
    conductor_->complete_signaling = [this]() {
        sleep(3);
        Disconnect();
    };
    return *this;
}

SignalrService &SignalrService::AutoReconnect() {
    connection_.set_disconnected([this](std::exception_ptr exception) {
        try {
            if (exception) {
                std::rethrow_exception(exception);
            }

            std::cout << "[SignalR][WithReconnect]: normal disconnection successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[SignalR][WithReconnect]: exception about " << e.what() << std::endl;
            sleep(1);
            Connect();
        }
    });

    return *this;
}

void SignalrService::Start() {
    while (connection_.get_connection_state() != signalr::connection_state::connected) {
        std::cout << "[SignalR][Connect] start connecting!" << std::endl;
        std::promise<void> start_task;
        connection_.start([&start_task](std::exception_ptr exception) {
            try {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            } catch (const std::exception& e) {
                std::cout << "[SignalR][Connect] exception: " << e.what() << std::endl;
                sleep(1);
            }
            start_task.set_value();   
        });

        start_task.get_future().get();
    }

    if (connected_func_) {
        connected_func_();
    }
};

void SignalrService::Disconnect() {
    std::promise<void> stop_task;
    connection_.stop([&stop_task](std::exception_ptr exception)
                     { stop_task.set_value(); });
    stop_task.get_future().get();
};

void SignalrService::Subscribe(std::string event_name,
                             const signalr::hub_connection::method_invoked_handler &handler) {
    connection_.on(event_name, handler);
};

void SignalrService::SendMessage(std::string method, std::vector<signalr::value> args) {
    if (connection_.get_connection_state() == signalr::connection_state::connected) {
        std::promise<void> send_task;
        connection_.invoke(method, args,
                        [&send_task](const signalr::value &value, std::exception_ptr exception)
                        {
                            send_task.set_value();
                        });
        send_task.get_future().get();
    } else {
        std::cout << "[SignalR][SendMessage]: not send msg due to disconnection" << std::endl;
    }
};

void SignalrService::JoinAsClient() {
    // todo
    // std::vector<signalr::value> args{};
    // SendMessage(topics.join_as_client, args);
};

void SignalrService::JoinAsServer() {
    std::cout << "=> SignalrService: start setting!" << std::endl;
    ListenClientId();
    ListenOfferSdp();
    ListenOfferIce();

    connected_func_ = [this]() {
        std::vector<signalr::value> args{};
        SendMessage(topics.join_as_server , args);
    };
};

void SignalrService::Connect() {
    JoinAsServer();
    Start();
}
