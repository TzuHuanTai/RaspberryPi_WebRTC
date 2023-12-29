#include "signaling_service.h"

SignalingService::SignalingService(OnRemoteSdpFunc on_remote_sdp, OnRemoteIceFunc on_remote_ice)
    : on_remote_sdp_(on_remote_sdp),
      on_remote_ice_(on_remote_ice) {}

void SignalingService::OnRemoteSdp(std::string sdp) {
    if (on_remote_sdp_) {
        on_remote_sdp_(sdp);
    }
}

void SignalingService::OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) {
    if (on_remote_ice_) {
        on_remote_ice_(sdp_mid, sdp_mline_index, candidate);
    }
}
