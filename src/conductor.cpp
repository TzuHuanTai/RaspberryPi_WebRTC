#include "conductor.h"
#include "v4l2_capture.h"
#include "customized_video_encoder_factory.h"

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <pc/video_track_source_proxy.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/include/audio_device_factory.h>
#include <modules/audio_device/linux/audio_device_alsa_linux.h>
#include <modules/audio_device/linux/audio_device_pulse_linux.h>
#include <modules/audio_device/linux/latebindingsymboltable_linux.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>

Conductor::Conductor(Args args) : args(args)
{
    std::cout << "=> Conductor: init" << std::endl;
    if (!InitializePeerConnection())
    {
        std::cout << "=> InitializePeerConnection: failed!" << std::endl;
    }

    if (!InitializeTracks())
    {
        std::cout << "=> InitializeTracks: failed!" << std::endl;
    }
}

bool Conductor::InitializeTracks()
{
    auto options = peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());
    audio_track_ =
        peer_connection_factory_->CreateAudioTrack("my_audio_label", options.get());

    auto video_track_source = V4L2Capture::Create(args.device);
    (*video_track_source)
        .UseRawBuffer(args.use_h264_hw_encoder)
        .SetFps(args.fps)
        .SetRotation(args.rotation_angle)
        .SetFormat(args.width, args.height, args.use_i420_src)
        .StartCapture();

    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source =
        webrtc::VideoTrackSourceProxy::Create(
            signaling_thread_.get(), worker_thread_.get(), video_track_source);
    video_track_ =
        peer_connection_factory_->CreateVideoTrack("my_video_label", video_source.get());

    return video_track_ != nullptr && audio_track_ != nullptr;
}

void Conductor::AddTracks()
{
    if (!peer_connection_->GetSenders().empty())
    {
        std::cout << "=> AddTracks: already add tracks." << std::endl;
        return;
    }

    std::string stream_id = "test_stream_id";

    auto audio_res = peer_connection_->AddTrack(audio_track_, {stream_id});
    if (!audio_res.ok())
    {
        std::cout << "=> AddTracks: audio_track_ failed" << audio_res.error().message() << std::endl;
    }

    auto video_res = peer_connection_->AddTrack(video_track_, {stream_id});
    if (!video_res.ok())
    {
        std::cout << "=> AddTracks: video_track_ failed" << video_res.error().message() << std::endl;
    }

    video_sender_ = video_res.value();
    webrtc::RtpParameters parameters = video_sender_->GetParameters();
    parameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    video_sender_->SetParameters(parameters);
}

bool Conductor::CreatePeerConnection()
{
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = args.stun_url;
    config.servers.push_back(server);

    webrtc::PeerConnectionDependencies dependencies(this);
    peer_connection_ = peer_connection_factory_->CreatePeerConnectionOrError(config, std::move(dependencies)).value();

    AddTracks();

    return peer_connection_ != nullptr;
}

bool Conductor::InitializePeerConnection()
{
    rtc::InitializeSSL();

    network_thread_ = rtc::Thread::CreateWithSocketServer();
    worker_thread_ = rtc::Thread::Create();
    signaling_thread_ = rtc::Thread::Create();

    if (network_thread_->Start())
    {
        std::cout << "=> network thread start: success!" << std::endl;
    }
    if (worker_thread_->Start())
    {
        std::cout << "=> worker thread start: success!" << std::endl;
    }
    if (signaling_thread_->Start())
    {
        std::cout << "=> signaling thread start: success!" << std::endl;
    }

    data_channel_subject_ = std::make_shared<DataChannelSubject>();

    std::unique_ptr<webrtc::VideoEncoderFactory> VideoEncoderFactory =
        args.use_h264_hw_encoder ? CreateCustomizedVideoEncoderFactory(data_channel_subject_)
                                 : webrtc::CreateBuiltinVideoEncoderFactory();

    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), worker_thread_.get(), signaling_thread_.get(), nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(),
        std::move(VideoEncoderFactory), webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr, nullptr);

    if (!peer_connection_factory_)
    {
        std::cout << "=> peer_connection_factory: failed" << std::endl;
    }
    else
    {
        std::cout << "=> peer_connection_factory: success!" << std::endl;
    }

    return peer_connection_factory_ != nullptr;
}

void Conductor::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
    std::cout << "=> OnSignalingChange: " << new_state << std::endl;
}

void Conductor::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
    data_channel_subject_->SetDataChannel(channel);
    std::cout << "=> OnDataChannel: connected to '" << channel->label() << "'" << std::endl;
}

void Conductor::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state)
{
    std::cout << "=> OnConnectionChange: " << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected)
    {
        std::unique_lock<std::mutex> lock(mtx);
        is_ready_for_streaming = true;
        cond_var.notify_all();

        if (complete_signaling)
        {
            complete_signaling();
        }
    }
    else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected)
    {
        peer_connection_->Close();
        data_channel_subject_->UnSubscribe();
    }
    else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed)
    {
        is_ready_for_streaming = false;
    }
}

void Conductor::OnStandardizedIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
    std::cout << "=> OnIceGatheringChange: " << new_state << std::endl;
}

void Conductor::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
    std::cout << "=> OnIceGatheringChange: " << new_state << std::endl;
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
    std::string ice;
    candidate->ToString(&ice);
    invoke_answer_ice(candidate->sdp_mid(), candidate->sdp_mline_index(), ice);
}

void Conductor::SetOfferSDP(const std::string sdp,
                            OnSetSuccessFunc on_success,
                            OnFailureFunc on_failure)
{
    std::cout << "=> SetOfferSDP: start" << std::endl;

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &error);
    if (!session_description)
    {
        RTC_LOG(LS_ERROR) << __FUNCTION__
                          << "Failed to create session description: "
                          << error.description.c_str()
                          << "\nline: " << error.line.c_str();
        return;
    }
    peer_connection_->SetRemoteDescription(
        SetSessionDescription::Create(std::move(on_success), std::move(on_failure)),
        session_description.release());
}

void Conductor::AddIceCandidate(std::string sdp_mid, int sdp_mline_index, std::string sdp)
{
    std::cout << "=> AddIceCandidate: start creating" << std::endl;
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, &error));
    if (!candidate.get())
    {
        RTC_LOG(LS_ERROR) << "Can't parse received candidate message: "
                          << error.description.c_str()
                          << "\nline: " << error.line.c_str();
        return;
    }
    std::cout << "=> AddIceCandidate: add candidated" << std::endl;
    peer_connection_->AddIceCandidate(
        std::move(candidate), [sdp](webrtc::RTCError error)
        { RTC_LOG(LS_WARNING)
              << __FUNCTION__ << " Failed to apply the received candidate. type="
              << webrtc::ToString(error.type()) << " message=" << error.message()
              << " sdp=" << sdp; });
    std::cout << "=> AddIceCandidate: end" << std::endl;
}

void Conductor::CreateAnswer(OnCreateSuccessFunc on_success, OnFailureFunc on_failure)
{
    auto with_set_local_desc = [this, on_success = std::move(on_success)](
                                   webrtc::SessionDescriptionInterface *desc)
    {
        std::string sdp;
        desc->ToString(&sdp);
        peer_connection_->SetLocalDescription(
            SetSessionDescription::Create(nullptr, nullptr), desc);
        if (on_success)
        {
            on_success(desc);
        }
    };
    peer_connection_->CreateAnswer(
        CreateSessionDescription::Create(std::move(with_set_local_desc), std::move(on_failure)),
        webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

Conductor::~Conductor()
{
    audio_track_ = nullptr;
    video_track_ = nullptr;
    peer_connection_factory_ = nullptr;
    network_thread_->Stop();
    worker_thread_->Stop();
    signaling_thread_->Stop();
    rtc::CleanupSSL();
    std::cout << "=> ~Conductor: destroied" << std::endl;
}
