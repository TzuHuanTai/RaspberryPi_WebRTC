#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include <functional>
#include <string>

class SignalingMessageObserver {
  public:
    using OnLocalSdpFunc = std::function<void(const std::string &peer_id, const std::string &sdp,
                                              const std::string &type)>;
    using OnLocalIceFunc =
        std::function<void(const std::string &peer_id, const std::string &sdp_mid,
                           int sdp_mline_index, const std::string &candidate)>;

    virtual void SetRemoteSdp(const std::string &sdp, const std::string &type) = 0;
    virtual void SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                              const std::string &candidate) = 0;

    void OnLocalSdp(OnLocalSdpFunc func) { on_local_sdp_fn_ = std::move(func); };
    void OnLocalIce(OnLocalIceFunc func) { on_local_ice_fn_ = std::move(func); };

  protected:
    OnLocalSdpFunc on_local_sdp_fn_ = nullptr;
    OnLocalIceFunc on_local_ice_fn_ = nullptr;
};

class SignalingService {
  public:
    SignalingService(){};
    virtual ~SignalingService(){};

    void SetPeerToMap(const std::string &peer_id, SignalingMessageObserver *callback) {
        peer_callback_map[peer_id] = callback;
    };
    void RemovePeerFromMap(const std::string &peer_id) {
        peer_callback_map.erase(peer_id);
    }
    void ResetPeerMap() { peer_callback_map.clear(); };

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

  protected:
    std::unordered_map<std::string, SignalingMessageObserver *> peer_callback_map;
};

#endif
