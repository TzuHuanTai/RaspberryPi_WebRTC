#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include <functional>
#include <string>

class SignalingMessageObserver {
  public:
    virtual void OnRemoteSdp(std::string sdp, std::string type) = 0;
    virtual void OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) = 0;
};

class SignalingService {
  public:
    SignalingService(){};
    virtual ~SignalingService(){};

    void SetCallback(std::string peer_id, SignalingMessageObserver *callback) {
        peer_callback_map[peer_id] = callback;
    };
    void RemoveCallback(std::string peer_id) {
        printf("%s callback is removed!\n", peer_id.c_str());
        peer_callback_map.erase(peer_id);
    }
    void ResetCallback() { peer_callback_map.clear(); };

    virtual void AnswerLocalSdp(std::string peer_id, std::string sdp, std::string type) = 0;
    virtual void AnswerLocalIce(std::string peer_id, std::string sdp_mid, int sdp_mline_index,
                                std::string candidate) = 0;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

  protected:
    std::unordered_map<std::string, SignalingMessageObserver *> peer_callback_map;
};

#endif
