#include "rtc_peer.h"

#include <thread>
#include <chrono>

#include "common/logging.h"
#if USE_MQTT_SIGNALING
#include "signaling/mqtt_service.h"
#endif

rtc::scoped_refptr<RtcPeer> RtcPeer::Create(Args args, int id) {
    auto ptr = rtc::make_ref_counted<RtcPeer>(id);
    ptr->InitSignalingClient(args);
    return ptr;
}

RtcPeer::RtcPeer(int id)
    : id_(id),
      is_connected_(false),
      is_complete_(false) {}

RtcPeer::~RtcPeer() {
    peer_connection_ = nullptr;
    signaling_client_.reset();
    data_channel_subject_.reset();
    timeout_thread_.join();
    DEBUG_PRINT("peer connection (%d) was destroyed!", id_);
}

int RtcPeer::GetId() const { return id_; }

bool RtcPeer::IsConnected() const { return is_connected_; }

void RtcPeer::SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj) {
    custom_video_sink_ = std::move(video_sink_obj);
}

void RtcPeer::SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer) {
    peer_connection_ = peer;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> RtcPeer::GetPeer() { return peer_connection_; }

void RtcPeer::InitSignalingClient(Args args) {
    signaling_client_ = ([args]() -> std::shared_ptr<SignalingService> {
#if USE_MQTT_SIGNALING
        return MqttService::Create(args);
#else
        return nullptr;
#endif
    })();
    signaling_client_->ResetCallback(this);
}

void RtcPeer::CreateDataChannel() {
    struct webrtc::DataChannelInit init;
    init.ordered = true;
    init.reliable = true;
    init.id = 0;
    auto result = peer_connection_->CreateDataChannelOrError("cmd_channel", &init);

    if (!result.ok()) {
        ERROR_PRINT("Failed to create data channel.");
        return;
    }

    DEBUG_PRINT("The data channel is established successfully.");
    data_channel_subject_ = std::make_shared<DataChannelSubject>();
    data_channel_subject_->SetDataChannel(result.MoveValue());

    auto conn_observer = data_channel_subject_->AsObservable(CommandType::CONNECT);
    conn_observer->Subscribe([this](std::string message) {
        if (message == "false") { // todo: use enum or so.
            peer_connection_->Close();
        }
    });
}

void RtcPeer::OnSnapshot(OnCommand func) { SubscribeCommandChannel(CommandType::SNAPSHOT, func); }

void RtcPeer::OnMetadata(OnCommand func) { SubscribeCommandChannel(CommandType::METADATA, func); }

void RtcPeer::OnRecord(OnCommand func) { SubscribeCommandChannel(CommandType::RECORD, func); }

void RtcPeer::SubscribeCommandChannel(CommandType type, OnCommand func) {
    auto observer = data_channel_subject_->AsObservable(type);
    observer->Subscribe([this, func](std::string message) {
        if (!message.empty()) {
            func(data_channel_subject_, message);
        }
    });
}

void RtcPeer::OnPaired(std::function<void(PeerState)> func) {
    auto observer = AsObservable();
    observer->Subscribe(func);
}

void RtcPeer::OnPaired(bool is_paired) {
    PeerState state = {
        .id = id_, .is_connected = is_connected_, .is_paired = is_paired};
    Next(state);
}

void RtcPeer::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnSignalingChange => %s", std::string(state).c_str());
    if (new_state == webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer) {
        OnPaired(true);
        timeout_thread_ = std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (!is_complete_ && !is_connected_) {
                DEBUG_PRINT("Connection timeout after kConnecting. Closing connection.");
                peer_connection_->Close();
            }
        });
    } else if (new_state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
        signaling_client_.reset();
    }
}

void RtcPeer::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    DEBUG_PRINT("Connected to data channel => %s", channel->label().c_str());
}

void RtcPeer::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnIceGatheringChange => %s", std::string(state).c_str());
}

void RtcPeer::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnConnectionChange => %s", std::string(state).c_str());
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
        signaling_client_.reset();
        is_connected_ = true;
        UnSubscribe();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed) {
        is_connected_ = false;
        peer_connection_->Close();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
        signaling_client_.reset();
        is_connected_ = false;
        is_complete_ = true;
        data_channel_subject_.reset();
    }
}

void RtcPeer::OnIceCandidate(const webrtc::IceCandidateInterface *candidate) {
    std::string ice;
    candidate->ToString(&ice);
    if (signaling_client_) {
        signaling_client_->AnswerLocalIce(candidate->sdp_mid(), candidate->sdp_mline_index(), ice);
    }
}

void RtcPeer::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    if (transceiver->receiver()->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO &&
        custom_video_sink_) {
        auto track = transceiver->receiver()->track();
        auto remote_video_track = static_cast<webrtc::VideoTrackInterface *>(track.get());
        DEBUG_PRINT("OnTrack => custom sink(%s) is added!", track->id().c_str());
        remote_video_track->AddOrUpdateSink(custom_video_sink_, rtc::VideoSinkWants());
    }
}

void RtcPeer::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
    peer_connection_->SetLocalDescription(SetSessionDescription::Create(nullptr, nullptr).get(),
                                          desc);

    std::string sdp;
    desc->ToString(&sdp);
    std::string type = webrtc::SdpTypeToString(desc->GetType());
    if (signaling_client_) {
        signaling_client_->AnswerLocalSdp(sdp, type);
    }
}

void RtcPeer::OnFailure(webrtc::RTCError error) {
    auto type = ToString(error.type());
    ERROR_PRINT("%s; %s", std::string(type).c_str(), error.message());
}

void RtcPeer::OnRemoteSdp(std::string sdp, std::string sdp_type) {
    if (is_connected_) {
        return;
    }

    absl::optional<webrtc::SdpType> type_maybe = webrtc::SdpTypeFromString(sdp_type);
    if (!type_maybe) {
        ERROR_PRINT("Unknown SDP type: %s", sdp_type.c_str());
        return;
    }
    webrtc::SdpType type = *type_maybe;

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
        ERROR_PRINT("Can't parse received session description message. %s",
                    error.description.c_str());
        return;
    }

    peer_connection_->SetRemoteDescription(SetSessionDescription::Create(nullptr, nullptr).get(),
                                           session_description.release());

    if (type == webrtc::SdpType::kOffer) {
        peer_connection_->CreateAnswer(this,
                                       webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }
}

void RtcPeer::OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string sdp) {
    if (is_connected_) {
        return;
    }

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, &error));
    if (!candidate.get()) {
        ERROR_PRINT("Can't parse received candidate message. %s", error.description.c_str());
        return;
    }

    if (!peer_connection_->AddIceCandidate(candidate.get())) {
        ERROR_PRINT("Failed to apply the received candidate!");
        return;
    }
}
