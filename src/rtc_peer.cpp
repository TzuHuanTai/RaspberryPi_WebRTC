#include "rtc_peer.h"

#include "common/base64_utils.h"

rtc::scoped_refptr<RtcPeer> RtcPeer::Create(Args args, int id) {
    auto ptr = rtc::make_ref_counted<RtcPeer>(id);
    ptr->args_ = args;
    return ptr;
}

RtcPeer::RtcPeer(int id)
    : is_connected_(false),
      id_(id) {}

RtcPeer::~RtcPeer() {
    peer_connection_ = nullptr;
    std::cout << "[RtcPeer] ("<< id_ <<") was destroyed!" << std::endl;
}

int RtcPeer::GetId() const {
    return id_;
}

bool RtcPeer::IsConnected() const {
    return is_connected_;
}

void RtcPeer::SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj) {
    custom_video_sink_ = std::move(video_sink_obj);
}

void RtcPeer::SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer) {
    peer_connection_ = peer;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> RtcPeer::GetPeer() {
    return peer_connection_;
}

void RtcPeer::SetSignalingService(std::shared_ptr<SignalingService> service) {
    signaling_service_ = service;
    signaling_service_->ResetCallback(this);
}

void RtcPeer::CreateDataChannel() {
    data_channel_subject_ = std::make_unique<DataChannelSubject>();

    struct webrtc::DataChannelInit init;
    init.ordered = true;
    init.reliable = true;
    init.id = 0;
    auto result = peer_connection_->CreateDataChannelOrError("cmd_channel", &init);

    if (result.ok()) {
        std::cout << "[RtcPeer] Succeeds to create data channel" << std::endl;
        data_channel_subject_->SetDataChannel(result.MoveValue());

        auto conn_observer = data_channel_subject_->AsObservable(CommandType::CONNECT);
        conn_observer->Subscribe([this](char *message) {
            if (strcmp(message, "false") == 0) {
                peer_connection_->Close();
            }
        });

        auto thumb_observer = data_channel_subject_->AsObservable(CommandType::THUMBNAIL);
        thumb_observer->Subscribe([this](char *message) {
            if (strcmp(message, "get") == 0 && !args_.record_path.empty()) {
                try {
                    auto latest_jpg_path = Base64Utils::FindLatestJpg(args_.record_path);
                    auto binary_data = Base64Utils::ReadBinary(latest_jpg_path);
                    auto base64_data = Base64Utils::Encode(binary_data);
                    std::cout << "Send Image: " << latest_jpg_path << std::endl;
                    std::string data_uri = "data:image/jpeg;base64," + base64_data;
                    data_channel_subject_->Send(CommandType::THUMBNAIL, data_uri);
                } catch (const std::exception &e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }
            }
        });

    } else {
        std::cout << "[RtcPeer] Fails to create data channel" << std::endl;
    }
}

void RtcPeer::OnReadyToConnect(std::function<void(PeerState)> func) {
    auto observer = AsObservable();
    observer->Subscribe(func);
}

void RtcPeer::EmitReadyToConnect(bool is_ready) {
    is_ready_to_connect_ = is_ready;
    PeerState state = {
        .id = id_,
        .isConnected = is_connected_,
        .isReadyToConnect = is_ready_to_connect_
    };
    Next(state);
}

void RtcPeer::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
    std::cout << "[RtcPeer] OnSignalingChange: ";
    std::cout << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
        signaling_service_.reset();
    }
}

void RtcPeer::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    std::cout << "[RtcPeer] OnDataChannel: connected to '" << channel->label() << "'" << std::endl;
}

void RtcPeer::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    std::cout << "[RtcPeer] OnIceGatheringChange: " << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering) {
        EmitReadyToConnect(true);
    }
}

void RtcPeer::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
    std::cout << "[RtcPeer] OnConnectionChange: " << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
        signaling_service_->ResetCallback();
        is_connected_ = true;
        EmitReadyToConnect(false);
        UnSubscribe();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed) {
        is_connected_ = false;
        peer_connection_->Close();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
        is_connected_ = false;
        data_channel_subject_.reset();
    }
}

void RtcPeer::OnIceCandidate(const webrtc::IceCandidateInterface *candidate) {
    std::string ice;
    candidate->ToString(&ice);
    if (signaling_service_) {
        signaling_service_->AnswerLocalIce(candidate->sdp_mid(), candidate->sdp_mline_index(), ice);
    }
}

void RtcPeer::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    if (transceiver->receiver()->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO
        && custom_video_sink_) {
        auto track = transceiver->receiver()->track();
        auto remote_video_track = static_cast<webrtc::VideoTrackInterface*>(track.get());
        std::cout << "[RtcPeer] OnTrack: custom sink is added!" << std::endl;
        remote_video_track->AddOrUpdateSink(custom_video_sink_, rtc::VideoSinkWants());
    }
}

void RtcPeer::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
        SetSessionDescription::Create(nullptr, nullptr).get(), desc);

    std::string sdp;
    desc->ToString(&sdp);
    std::string type = webrtc::SdpTypeToString(desc->GetType());
    if (signaling_service_) {
        signaling_service_->AnswerLocalSdp(sdp, type);
    }
}

void RtcPeer::OnFailure(webrtc::RTCError error) {
    std::cout << ToString(error.type()) << ": " << error.message() << std::endl;
}

void RtcPeer::OnRemoteSdp(std::string sdp, std::string sdp_type) {
    if (is_connected_) {
        return;
    }

    absl::optional<webrtc::SdpType> type_maybe =
        webrtc::SdpTypeFromString(sdp_type);
    if (!type_maybe) {
      std::cout << "[RtcPeer] Unknown SDP type: " << sdp_type << std::endl;
      return;
    }
    webrtc::SdpType type = *type_maybe;

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
        std::cout << "[RtcPeer] Can't parse received session description message. \n"
                  << error.description.c_str() << std::endl;
        return;
    }

    peer_connection_->SetRemoteDescription(
        SetSessionDescription::Create(nullptr, nullptr).get(),
        session_description.release());

    if (type == webrtc::SdpType::kOffer) {
        peer_connection_->CreateAnswer(
            this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
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
        std::cout << "[RtcPeer] Can't parse received candidate message. \n"
                  << error.description.c_str() << std::endl;
        return;
    }

    if (!peer_connection_->AddIceCandidate(candidate.get())) {
        std::cout << "[RtcPeer] Failed to apply the received candidate" << std::endl;
        return;
    }
}
