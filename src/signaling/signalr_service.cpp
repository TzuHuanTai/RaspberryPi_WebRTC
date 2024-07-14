#include "signaling/signalr_service.h"
#include "common/logging.h"

#include <future>
#include <sstream>
#include <map>
#include <unistd.h>

std::shared_ptr<SignalrService> SignalrService::Create(Args args) {
    auto ptr = std::make_shared<SignalrService>(args);
    ptr->AutoReconnect()
        .Connect();
    return ptr;
}

SignalrService::SignalrService(Args args)
    : SignalingService(),
      url(args.signaling_url),
      connection_(signalr::hub_connection_builder::create(url).build()) {}

void SignalrService::ListenClientId() {
    Subscribe(topics.connected_client, [this](const std::vector<signalr::value> &m) {
        std::unique_lock<std::mutex> lock(mtx);
        if (client_id_.empty()) {
            client_id_ = m[0].as_string();
        } else {
            DEBUG_PRINT("Client %s want to replace exist client %s", m[0].as_string().c_str(), client_id_.c_str(), );
        }
        DEBUG_PRINT("Connected client %s", client_id_.c_str(), );
        ready = true;
        cond_var.notify_all();
    });
}

void SignalrService::ListenOfferSdp() {
    Subscribe(topics.offer_sdp, [this](const std::vector<signalr::value> &m) {
        const std::map<std::string, signalr::value> object = m[0].as_map();

        std::string sdp;
        std::string type;
        for (const auto &s : object) {
            if (s.first == "sdp") {
                sdp = s.second.as_string();
            } else if (s.first == "type") {
                type = s.second.as_string();
            }
        }
        DEBUG_PRINT("Receive sdp: %s, type: %s", sdp.c_str(), type.c_str());
        if (callback_) {
            callback_->OnRemoteSdp(sdp, type);
        }
    });
}

void SignalrService::ListenOfferIce() {
    Subscribe(topics.offer_ice, [this](const std::vector<signalr::value> &m) {
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
        DEBUG_PRINT("Receive ICE: %s, %d, %s", sdp_mline_index.c_str(), sdp_mid, candidate.c_str());
        if (callback_) {
            callback_->OnRemoteIce(sdp_mid, sdp_mline_index, candidate);
        }
    });
}

void SignalrService::AnswerLocalSdp(std::string sdp, std::string type) {
    std::unique_lock<std::mutex> lock(mtx);
    DEBUG_PRINT("Invoke AnswerSDP: %s", sdp.c_str());
    std::map<std::string, signalr::value> sdp_message = {
            {"sdp", sdp},
            {"type", type}};

    DEBUG_PRINT("Invoke AnswerSDP: wait!");
    cond_var.wait(lock, [this]{ return ready; });
    DEBUG_PRINT("Invoke AnswerSDP: got clientId %s", client_id_.c_str());
        
    std::vector<signalr::value> args{client_id_, sdp_message};
    SendMessage(topics.answer_sdp, args);
}

void SignalrService::AnswerLocalIce(std::string sdp_mid, int sdp_mline_index,
                                  std::string candidate) {
    std::unique_lock<std::mutex> lock(mtx);
    DEBUG_PRINT("Invoke AnswerICE: %s, %d, %s", sdp_mid.c_str(), sdp_mline_index, candidate.c_str());
    std::map<std::string, signalr::value> ice_message = {
            {"sdpMid", sdp_mid},
            {"sdpMLineIndex", static_cast<double>(sdp_mline_index)},
            {"candidate", candidate}};

    DEBUG_PRINT("Invoke AnswerICE: wait!");
    cond_var.wait(lock, [this]{ return ready; });
    DEBUG_PRINT("Invoke AnswerICE: got clientId %s", client_id_.c_str());
        
    std::vector<signalr::value> args{client_id_, ice_message};
    SendMessage(topics.answer_ice, args);
}

SignalrService &SignalrService::AutoReconnect() {
    connection_.set_disconnected([this](std::exception_ptr exception) {
        try {
            if (exception) {
                std::rethrow_exception(exception);
            }

            DEBUG_PRINT("Normal disconnection successfully!");
        } catch (const std::exception& e) {
            ERROR_PRINT("Exception about %s", e.what());
            sleep(1);
            Connect();
        }
    });

    return *this;
}

void SignalrService::Start() {
    while (connection_.get_connection_state() != signalr::connection_state::connected) {
        ERROR_PRINT("Start connecting!");
        std::promise<void> start_task;
        connection_.start([&start_task](std::exception_ptr exception) {
            try {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            } catch (const std::exception& e) {
                ERROR_PRINT("%s", e.what());
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
        ERROR_PRINT("Not send msg due to disconnection");
    }
};

void SignalrService::JoinAsClient() {
    // todo
    // std::vector<signalr::value> args{};
    // SendMessage(topics.join_as_client, args);
};

void SignalrService::JoinAsServer() {
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
