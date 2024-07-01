#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_

#include "signaling/signaling_service.h"
#include "args.h"
#include <memory>
#include <mosquitto.h>

class MqttService : public SignalingService {
public:
    static std::shared_ptr<MqttService> Create(Args args);

    MqttService(Args args);
    ~MqttService() override;

    void Connect() override;
    void Disconnect() override;
    void AnswerLocalSdp(std::string sdp, std::string type) override;
    void AnswerLocalIce(std::string sdp_mid,
                                int sdp_mline_index,
                                std::string candidate) override;

private:
    int port_;
    bool sdp_received_;
    std::string uid_;
    std::string hostname_;
    std::string username_;
    std::string password_;
    std::string sdp_base_topic_;
    std::string ice_base_topic_;
    std::string remote_client_id_;
    struct mosquitto *connection_;
    void ListenOfferSdp(std::string message);
    void ListenOfferIce(std::string message);
    void Subscribe(const std::string& topic);
    void Publish(const std::string& topic, const std::string& msg);
    void OnConnect(struct mosquitto *mosq, void *obj, int result);
    void OnMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
    std::string GetClientId(std::string& topic);
    std::string GetTopic(const std::string& topic, const std::string& client_id = "") const;
};

#endif
