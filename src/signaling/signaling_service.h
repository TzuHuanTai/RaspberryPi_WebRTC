#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include <functional>
#include <string>

#include "common/logging.h"
#include "conductor.h"

class SignalingService {
  public:
    SignalingService(std::shared_ptr<Conductor> conductor, bool is_candidates_in_sdp = false)
        : conductor_(conductor),
          is_candidates_in_sdp_(is_candidates_in_sdp){};
    virtual ~SignalingService(){};

    rtc::scoped_refptr<RtcPeer> CreatePeer() {
        auto peer = conductor_->CreatePeerConnection(is_candidates_in_sdp_);
        peer_map_[peer->GetId()] = peer;
        return peer;
    };

    rtc::scoped_refptr<RtcPeer> GetPeer(const std::string &peer_id) {
        auto it = peer_map_.find(peer_id);
        if (it != peer_map_.end()) {
            return it->second;
        }
        return nullptr;
    };
    bool ContainsInPeerMap(const std::string &peer_id) { return peer_map_.contains(peer_id); };
    void RemovePeerFromMap(const std::string &peer_id) { peer_map_.erase(peer_id); };
    void RefreshPeerMap() {
        auto pm_it = peer_map_.begin();
        while (pm_it != peer_map_.end()) {
            auto peer_id = pm_it->second->GetId();
            DEBUG_PRINT("Refresh peer_map key: %s, value: %d", peer_id.c_str(),
                        pm_it->second->IsConnected());

            if (pm_it->second && !pm_it->second->IsConnected()) {
                pm_it = peer_map_.erase(pm_it);
                DEBUG_PRINT("peer_map (%s) was erased.", peer_id.c_str());
            } else {
                ++pm_it;
            }
        }
    };
    void ResetPeerMap() { peer_map_.clear(); };

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

  private:
    bool is_candidates_in_sdp_;
    std::shared_ptr<Conductor> conductor_;
    std::unordered_map<std::string, rtc::scoped_refptr<RtcPeer>> peer_map_;
};

#endif
