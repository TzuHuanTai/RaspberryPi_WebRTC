#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_

#include "signaling/signaling_service.h"

#include <memory>
#include <mosquitto.h>

#include "args.h"
#include <conductor.h>

class MqttService : public std::enable_shared_from_this<MqttService>,
                    public SignalingService {
  public:
    static std::shared_ptr<MqttService> Create(Args args, std::shared_ptr<Conductor> conductor);

    MqttService(Args args, std::shared_ptr<Conductor> conductor);
    ~MqttService() override;

    void Connect() override;
    void Disconnect() override;
    void AnswerLocalSdp(std::string peer_id, std::string sdp, std::string type) override;
    void AnswerLocalIce(std::string peer_id, std::string sdp_mid, int sdp_mline_index,
                        std::string candidate) override;

  private:
    int port_;
    std::string uid_;
    std::string hostname_;
    std::string username_;
    std::string password_;
    std::string sdp_base_topic_;
    std::string ice_base_topic_;
    struct mosquitto *connection_;
    std::shared_ptr<Conductor> conductor_;

    std::unordered_map<std::string, std::string> peer_id_to_client_id_;
    std::unordered_map<std::string, rtc::scoped_refptr<RtcPeer>> client_id_to_peer_;
    void RefreshPeerMap();

    void OnRemoteSdp(std::string peer_id, std::string message);
    void OnRemoteIce(std::string peer_id, std::string message);
    void Subscribe(const std::string &topic);
    void Unsubscribe(const std::string &topic);
    void Publish(const std::string &topic, const std::string &msg);
    void OnConnect(struct mosquitto *mosq, void *obj, int result);
    void OnMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
    std::string GetClientId(std::string &topic);
    std::string GetTopic(const std::string &topic, const std::string &client_id = "") const;
};

#endif
