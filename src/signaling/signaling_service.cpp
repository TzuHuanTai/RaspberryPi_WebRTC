#include "signaling_service.h"

SignalingService::SignalingService(std::shared_ptr<Conductor> conductor) 
    : conductor_(conductor) {
    InitIceCallback();
}

void SignalingService::OnRemoteSdp(std::string sdp) {
    if (!conductor_) return;
    conductor_->SetOfferSDP(sdp, [this]() {
        conductor_->CreateAnswer([this](webrtc::SessionDescriptionInterface *desc) {
            std::string answer_sdp;
            desc->ToString(&answer_sdp);
            AnswerLocalSdp(answer_sdp);
        }, nullptr);
    }, nullptr);
}

void SignalingService::OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) {
    // bug: Failed to apply the received candidate. connect but without datachannel!?
    // conductor_->AddIceCandidate(sdp_mid, sdp_mline_index, candidate);
}

void SignalingService::InitIceCallback() {
    if (!conductor_) return;
    conductor_->invoke_answer_ice = 
        [this](std::string sdp_mid, int sdp_mline_index, std::string candidate) {
            AnswerLocalIce(sdp_mid, sdp_mline_index, candidate);
        };
}
