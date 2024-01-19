#include "signaling/mqtt_service.h"
#include "args.h"

#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <map>
#include <unistd.h>
#include <nlohmann/json.hpp>

std::unique_ptr<MqttService> MqttService::Create(
    Args args, SignalingMessageObserver* callback) {
    auto ptr = std::make_unique<MqttService>(args, callback);
    ptr->Connect();
    return ptr;
}

MqttService::MqttService(Args args, SignalingMessageObserver* callback)
    : SignalingService(callback),
      port_(args.mqtt_port),
      hostname_(args.mqtt_host),
      username_(args.mqtt_username),
      password_(args.mqtt_password),
      connection_(nullptr) {}

MqttService::~MqttService() {
    Disconnect();
}

void MqttService::ListenOfferSdp(std::string message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp = jsonObj["sdp"];
    std::cout << "Received OfferSDP: " << sdp << std::endl;
    callback_->OnRemoteSdp(sdp);
}

void MqttService::ListenOfferIce(std::string message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp_mid = jsonObj["sdpMid"];
    int sdp_mline_index = jsonObj["sdpMLineIndex"];
    std::string candidate = jsonObj["candidate"];
    std::cout << "Receive OfferICE: " << sdp_mline_index << ", " << sdp_mid << ", " << candidate << std::endl;
    callback_->OnRemoteIce(sdp_mid, sdp_mline_index, candidate);
}

void MqttService::AnswerLocalSdp(std::string sdp) {
    std::cout << "AnswerSDP: " << sdp << std::endl;
    nlohmann::json jsonData;
    jsonData["type"] = "answer";
    jsonData["sdp"] = sdp;
    std::string jsonString = jsonData.dump();

    int rc = mosquitto_publish(connection_, NULL, topics.answer_sdp.c_str(), 
                            jsonString.length(), jsonString.c_str(), 2, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
    }
}

void MqttService::AnswerLocalIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) {
    std::cout << "AnswerICE: " << sdp_mid << ", " << sdp_mline_index << ", " << candidate << std::endl;
    nlohmann::json jsonData;
    jsonData["sdpMid"] = sdp_mid;
    jsonData["sdpMLineIndex"] = sdp_mline_index;
    jsonData["candidate"] = candidate;
    std::string jsonString = jsonData.dump();

    int rc = mosquitto_publish(connection_, NULL, topics.answer_ice.c_str(), 
                            jsonString.length(), jsonString.c_str(), 2, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
    }
}

void MqttService::Disconnect() {
    mosquitto_disconnect(connection_);

    if (connection_) {
        mosquitto_destroy(connection_);
        connection_ = nullptr;
    }
    mosquitto_lib_cleanup();
    std::cout << "MQTT service is released." << std::endl;
};

void MqttService::Subscribe(const std::string& topic) {
    int subscribe_result = mosquitto_subscribe(connection_, nullptr, topic.c_str(), 0);
    if (subscribe_result == MOSQ_ERR_SUCCESS) {
        std::cout << "Successfully subscribed to topic: " << topic << std::endl;
    } else {
        std::cout << "Failed to subscribe topic: " << topic << std::endl;
    }
}

void MqttService::OnConnect(struct mosquitto *mosq, void *obj, int result) {
    if (result == 0) {
        Subscribe(topics.offer_sdp);
        Subscribe(topics.offer_ice);
        std::cout << "MQTT service is ready." << std::endl;
    } else {
        // todo: retry connection on failure
    }
}

void MqttService::OnMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    if (!message->payload) return;

    std::string topic(message->topic);
    std::string payload(static_cast<char*>(message->payload));

    // todo: use map to run the fn of topics.
    if (topic == topics.offer_sdp) {
        ListenOfferSdp(payload);
    } else if (topic == topics.offer_ice) {
        ListenOfferIce(payload);
    }
}

void MqttService::Connect() {
    mosquitto_lib_init();

    connection_ = mosquitto_new(nullptr, true, this);

    if (connection_ == nullptr){
        fprintf(stderr, "Error: Out of memory.\n");
    }

    if (!username_.empty()) {
        mosquitto_username_pw_set(connection_, username_.c_str(), password_.c_str());
    }

	/* Configure callbacks. This should be done before connecting ideally. */
    mosquitto_connect_callback_set(connection_, [](struct mosquitto *mosq, void *obj, int result) {
        MqttService *service = static_cast<MqttService*>(obj);
        service->OnConnect(mosq, obj, result);
    });
    mosquitto_message_callback_set(connection_, [](struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
        MqttService *service = static_cast<MqttService*>(obj);
        service->OnMessage(mosq, obj, message);
    });

    int rc = mosquitto_connect_async(connection_, hostname_.c_str(), port_, 60);
    if(rc != MOSQ_ERR_SUCCESS){
        mosquitto_destroy(connection_);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    }

    rc = mosquitto_loop_start(connection_); // already handle reconnections
    if(rc != MOSQ_ERR_SUCCESS){
        mosquitto_destroy(connection_);
        fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    }	
}
