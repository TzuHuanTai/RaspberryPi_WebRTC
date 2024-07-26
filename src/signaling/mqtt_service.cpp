#include "signaling/mqtt_service.h"

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mqtt_protocol.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unistd.h>

#include "args.h"
#include "common/logging.h"

std::shared_ptr<MqttService> MqttService::Create(Args args) {
    auto ptr = std::make_shared<MqttService>(args);
    ptr->Connect();
    return ptr;
}

MqttService::MqttService(Args args)
    : SignalingService(),
      port_(args.mqtt_port),
      sdp_received_(false),
      uid_(args.uid),
      hostname_(args.mqtt_host),
      username_(args.mqtt_username),
      password_(args.mqtt_password),
      sdp_base_topic_(GetTopic("sdp")),
      ice_base_topic_(GetTopic("ice")),
      connection_(nullptr) {}

std::string MqttService::GetTopic(const std::string &topic, const std::string &client_id) const {
    std::string result;
    if (!uid_.empty()) {
        result = uid_ + "/";
    }
    result += topic;
    if (!client_id.empty()) {
        result += "/" + client_id;
    }
    return result;
}

MqttService::~MqttService() { Disconnect(); }

void MqttService::ListenOfferSdp(std::string message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp = jsonObj["sdp"];
    std::string type = jsonObj["type"];
    DEBUG_PRINT("Received remote [%s] SDP: %s", type.c_str(), sdp.c_str());
    if (callback_) {
        callback_->OnRemoteSdp(sdp, type);
    }
}

void MqttService::ListenOfferIce(std::string message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp_mid = jsonObj["sdpMid"];
    int sdp_mline_index = jsonObj["sdpMLineIndex"];
    std::string candidate = jsonObj["candidate"];
    DEBUG_PRINT("Received remote ICE: %s, %d, %s", sdp_mid.c_str(), sdp_mline_index,
                candidate.c_str());
    if (callback_) {
        callback_->OnRemoteIce(sdp_mid, sdp_mline_index, candidate);
    }
}

void MqttService::AnswerLocalSdp(std::string sdp, std::string type) {
    DEBUG_PRINT("Answer local [%s] SDP: %s", type.c_str(), sdp.c_str());
    nlohmann::json jsonData;
    jsonData["type"] = type;
    jsonData["sdp"] = sdp;
    std::string jsonString = jsonData.dump();

    Publish(GetTopic("sdp", remote_client_id_), jsonString);
}

void MqttService::AnswerLocalIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) {
    DEBUG_PRINT("Sent local ICE:  %s, %d, %s", sdp_mid.c_str(), sdp_mline_index, candidate.c_str());
    nlohmann::json jsonData;
    jsonData["sdpMid"] = sdp_mid;
    jsonData["sdpMLineIndex"] = sdp_mline_index;
    jsonData["candidate"] = candidate;
    std::string jsonString = jsonData.dump();

    Publish(GetTopic("ice", remote_client_id_), jsonString);
}

void MqttService::Disconnect() {
    mosquitto_disconnect(connection_);

    if (connection_) {
        mosquitto_destroy(connection_);
        connection_ = nullptr;
    }
    mosquitto_lib_cleanup();
    DEBUG_PRINT("MQTT service is released.");
};

void MqttService::Publish(const std::string &topic, const std::string &msg) {
    int rc =
        mosquitto_publish(connection_, NULL, topic.c_str(), msg.length(), msg.c_str(), 2, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("Error publishing: %s", mosquitto_strerror(rc));
    }
}

void MqttService::Subscribe(const std::string &topic) {
    int subscribe_result = mosquitto_subscribe_v5(connection_, nullptr, topic.c_str(), 0,
                                                  MQTT_SUB_OPT_NO_LOCAL, nullptr);
    if (subscribe_result == MOSQ_ERR_SUCCESS) {
        DEBUG_PRINT("Successfully subscribed to topic: %s", topic.c_str());
    } else {
        DEBUG_PRINT("Failed to subscribe topic: %s", topic.c_str());
    }
}

void MqttService::OnConnect(struct mosquitto *mosq, void *obj, int result) {
    if (result == 0) {
        Subscribe(sdp_base_topic_ + "/+/offer");
        Subscribe(ice_base_topic_ + "/+/offer");
        DEBUG_PRINT("MQTT service is ready.");
    } else {
        // todo: retry connection on failure
        DEBUG_PRINT("Connect failed with error code: %d", result);
    }
}

void MqttService::OnMessage(struct mosquitto *mosq, void *obj,
                            const struct mosquitto_message *message) {
    if (!message->payload)
        return;

    std::string topic(message->topic);
    std::string payload(static_cast<char *>(message->payload));

    if (!sdp_received_ && topic.substr(0, sdp_base_topic_.length()) == sdp_base_topic_) {
        remote_client_id_ = GetClientId(topic);
        sdp_received_ = true;
        ListenOfferSdp(payload);
    } else if (sdp_received_ && topic.substr(0, ice_base_topic_.length()) == ice_base_topic_) {
        ListenOfferIce(payload);
    }
}

std::string MqttService::GetClientId(std::string &topic) {
    std::string base_topic;
    if (topic.find(sdp_base_topic_) == 0) {
        base_topic = sdp_base_topic_;
    } else if (topic.find(ice_base_topic_) == 0) {
        base_topic = ice_base_topic_;
    } else {
        return "";
    }
    size_t base_length = base_topic.length();
    size_t start_pos = base_length + 1; // skip the trailing "/" of base_topic
    size_t end_pos = topic.find('/', start_pos);

    if (end_pos != std::string::npos) {
        return topic.substr(start_pos, end_pos - start_pos);
    }

    return "";
}

void MqttService::Connect() {
    mosquitto_lib_init();

    connection_ = mosquitto_new(uid_.c_str(), true, this);
    mosquitto_int_option(connection_, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
    if (port_ == 8883) {
        mosquitto_int_option(connection_, MOSQ_OPT_TLS_USE_OS_CERTS, 1);
    }

    if (connection_ == nullptr) {
        ERROR_PRINT("Failed to new mosquitto object.");
        return;
    }

    if (!username_.empty()) {
        mosquitto_username_pw_set(connection_, username_.c_str(), password_.c_str());
    }

    /* Configure callbacks. This should be done before connecting ideally. */
    mosquitto_connect_callback_set(connection_, [](struct mosquitto *mosq, void *obj, int result) {
        MqttService *service = static_cast<MqttService *>(obj);
        service->OnConnect(mosq, obj, result);
    });
    mosquitto_message_callback_set(connection_, [](struct mosquitto *mosq, void *obj,
                                                   const struct mosquitto_message *message) {
        MqttService *service = static_cast<MqttService *>(obj);
        service->OnMessage(mosq, obj, message);
    });

    int rc = mosquitto_connect_async(connection_, hostname_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(connection_);
        ERROR_PRINT("%s", mosquitto_strerror(rc));
    }

    rc = mosquitto_loop_start(connection_); // already handle reconnections
    if (rc != MOSQ_ERR_SUCCESS) {
        mosquitto_destroy(connection_);
        ERROR_PRINT("%s", mosquitto_strerror(rc));
    }
}
