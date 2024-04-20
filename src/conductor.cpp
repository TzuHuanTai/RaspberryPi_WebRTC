#include "conductor.h"
#include "capture/v4l2_capture.h"
#include "track/v4l2_track_source.h"
#include "track/v4l2dma_track_source.h"
#include "customized_video_encoder_factory.h"
#if USE_MQTT_SIGNALING
#include "signaling/mqtt_service.h"
#endif
#if USE_SIGNALR_SIGNALING
#include "signaling/signalr_service.h"
#endif

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/data_channel_interface.h>
#include <api/rtc_event_log/rtc_event_log_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "api/video_codecs/video_decoder_factory_template_open_h264_adapter.h"
#include <pc/video_track_source_proxy.h>
#include <media/engine/webrtc_media_engine.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/include/audio_device_factory.h>
#include <modules/audio_device/linux/audio_device_alsa_linux.h>
#include <modules/audio_device/linux/audio_device_pulse_linux.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>

rtc::scoped_refptr<Conductor> Conductor::Create(Args args) {
    auto ptr = rtc::make_ref_counted<Conductor>(args);
    if (!ptr->InitializePeerConnectionFactory()) {
        std::cout << "[Conductor] Initialize PeerConnection failed!" << std::endl;
    }

    ptr->InitializeTracks();

    if (!ptr->InitializeRecorder()) {
        std::cout << "[Conductor] Recorder is not created!" << std::endl;
    }
    return ptr;
}

Conductor::Conductor(Args args) : args(args) {}

bool Conductor::InitializeSignaling() {
    signaling_service_ = ([this]() -> std::unique_ptr<SignalingService> {
#if USE_MQTT_SIGNALING
        return MqttService::Create(args, this);
#elif USE_SIGNALR_SIGNALING
        return SignalrService::Create(args, this);
#else
        return nullptr;
#endif
    })();

    return signaling_service_ != nullptr;
}

bool Conductor::InitializeRecorder() {
    if (args.record_path.empty()) {
        return false;
    }

    bg_recorder_ = BackgroundRecorder::CreateBackgroundRecorder(
        video_caputre_source_, RecorderFormat::H264);
    bg_recorder_->Start();    

    return true;
}

void Conductor::InitializeTracks() {
    auto options = peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());
    audio_track_ =
        peer_connection_factory_->CreateAudioTrack("audio_track", options.get());

    if (args.device.empty()) {
            return;
    }
    /* split into capture and track source*/
    video_caputre_source_ = V4L2Capture::Create(args);
    auto video_track_source =  ([this]() -> rtc::scoped_refptr<rtc::AdaptedVideoTrackSource> {
        if (args.enable_v4l2_dma || args.v4l2_format == "h264") {
            return V4l2DmaTrackSource::Create(video_caputre_source_);
        } else {
            return V4L2TrackSource::Create(video_caputre_source_);
        }
    })();

    rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> video_source =
        webrtc::VideoTrackSourceProxy::Create(
            signaling_thread_.get(), worker_thread_.get(), video_track_source);
    video_track_ =
        peer_connection_factory_->CreateVideoTrack(video_source, "video_track");
}

void Conductor::AddTracks() {
    if (!peer_connection_->GetSenders().empty()) {
        std::cout << "=> AddTracks: already add tracks." << std::endl;
        return;
    }

    std::string stream_id = "test_stream_id";

    auto audio_res = peer_connection_->AddTrack(audio_track_, {stream_id});
    if (!audio_res.ok()) {
        std::cout << "=> AddTracks: audio_track_ failed" << audio_res.error().message() << std::endl;
    }

    if (!video_track_) {
        return;
    }
    auto video_res = peer_connection_->AddTrack(video_track_, {stream_id});
    if (!video_res.ok()) {
        std::cout << "=> AddTracks: video_track_ failed" << video_res.error().message() << std::endl;
    }

    video_sender_ = video_res.value();
    webrtc::RtpParameters parameters = video_sender_->GetParameters();
    parameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    video_sender_->SetParameters(parameters);
}

bool Conductor::CreatePeerConnection() {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = args.stun_url;
    config.servers.push_back(server);

    if (!args.turn_url.empty()) {
        webrtc::PeerConnectionInterface::IceServer turn_server;
        turn_server.uri = args.turn_url;
        turn_server.username = args.turn_username;
        turn_server.password = args.turn_password;
        config.servers.push_back(turn_server);
    }

    auto result = peer_connection_factory_->CreatePeerConnectionOrError(
        config, webrtc::PeerConnectionDependencies(this));

    if (!result.ok()) {
        peer_connection_ = nullptr;
        return false;
    } else {
        peer_connection_ = result.MoveValue();
        InitializeSignaling();
        CreateDataChannel();
        AddTracks();
    }
  
    return peer_connection_ != nullptr;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> Conductor::GetPeer() const {
    if (!peer_connection_) {
        std::cout << "failed: peer connection is not found!" << std::endl;
        return nullptr;
    }
    return peer_connection_;
}

void Conductor::SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj) {
    custom_video_sink_ = std::move(video_sink_obj);
}

void Conductor::CreateDataChannel()
{
    struct webrtc::DataChannelInit init;
    init.ordered = true;
    init.reliable = true;
    init.id = 0;
    auto result = peer_connection_->CreateDataChannelOrError("cmd_channel", &init);

    if (result.ok()) {
        std::cout << "Succeeds to create data channel" << std::endl;
        data_channel_subject_->SetDataChannel(result.MoveValue());
        auto observer = data_channel_subject_->AsObservable(CommandType::CONNECT);
        observer->Subscribe([this](char *message) {
            std::cout << "[OnDataChannel]: received msg => " << message << std::endl;
            if (strcmp(message, "false") == 0)
            {
                peer_connection_->Close();
                data_channel_subject_->UnSubscribe();
            }
        });
    }
    else
    {
        std::cout << "Fails to create data channel" << std::endl;
    }
}

bool Conductor::InitializePeerConnectionFactory() {
    rtc::InitializeSSL();

    network_thread_ = rtc::Thread::CreateWithSocketServer();
    worker_thread_ = rtc::Thread::Create();
    signaling_thread_ = rtc::Thread::Create();

    if (network_thread_->Start()) {
        std::cout << "=> network thread start: success!" << std::endl;
    }
    if (worker_thread_->Start()) {
        std::cout << "=> worker thread start: success!" << std::endl;
    }
    if (signaling_thread_->Start()) {
        std::cout << "=> signaling thread start: success!" << std::endl;
    }

    data_channel_subject_ = std::make_shared<DataChannelSubject>();

    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = network_thread_.get();;
    dependencies.worker_thread = worker_thread_.get();
    dependencies.signaling_thread = signaling_thread_.get();
    dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    dependencies.call_factory = webrtc::CreateCallFactory();
    dependencies.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>(
        dependencies.task_queue_factory.get());
    dependencies.trials = std::make_unique<webrtc::FieldTrialBasedConfig>();

    cricket::MediaEngineDependencies media_dependencies;
    media_dependencies.task_queue_factory = dependencies.task_queue_factory.get();

    media_dependencies.adm = webrtc::AudioDeviceModule::Create(
                    webrtc::AudioDeviceModule::kLinuxPulseAudio, 
                    dependencies.task_queue_factory.get());
    media_dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    media_dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    media_dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();
    media_dependencies.audio_mixer = nullptr;
    media_dependencies.video_encoder_factory = CreateCustomizedVideoEncoderFactory(args);
    media_dependencies.video_decoder_factory = std::make_unique<webrtc::VideoDecoderFactoryTemplate<
          webrtc::OpenH264DecoderTemplateAdapter,
          webrtc::LibvpxVp8DecoderTemplateAdapter,
          webrtc::LibvpxVp9DecoderTemplateAdapter,
          webrtc::Dav1dDecoderTemplateAdapter>>();
    media_dependencies.trials = dependencies.trials.get();
    dependencies.media_engine = cricket::CreateMediaEngine(std::move(media_dependencies));

    peer_connection_factory_ = CreateModularPeerConnectionFactory(std::move(dependencies));

    if (!peer_connection_factory_) {
        std::cout << "=> peer_connection_factory: failed" << std::endl;
    } else {
        std::cout << "=> peer_connection_factory: success!" << std::endl;
    }

    return peer_connection_factory_ != nullptr;
}

void Conductor::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    if (transceiver->receiver()->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO
        && custom_video_sink_) {
        auto track = transceiver->receiver()->track();
        auto remote_video_track = static_cast<webrtc::VideoTrackInterface*>(track.get());
        std::cout << "OnTrack: custom sink is added!" << std::endl;
        remote_video_track->AddOrUpdateSink(custom_video_sink_, rtc::VideoSinkWants());
    }
}

void Conductor::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
    std::cout << "[Condictor] OnSignalingChange: ";
    std::cout << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::SignalingState::kClosed) {
        signaling_service_.reset();
    }
}

void Conductor::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel)
{
    std::cout << "=> OnDataChannel: connected to '" << channel->label() << "'" << std::endl;
}

void Conductor::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
    std::cout << "=> OnConnectionChange: " << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
        signaling_service_.reset();
        is_connected = true;
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed) {
        peer_connection_->Close();
        data_channel_subject_->UnSubscribe();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
        SetStreamingState(false);
        is_connected = false;
    }
}

void Conductor::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    std::cout << "=> OnIceGatheringChange: " << webrtc::PeerConnectionInterface::PeerConnectionInterface::AsString(new_state) << std::endl;
    if (new_state == webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering) {
        SetStreamingState(true);
    }
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface *candidate) {
    std::string ice;
    candidate->ToString(&ice);
    signaling_service_->AnswerLocalIce(candidate->sdp_mid(), candidate->sdp_mline_index(), ice);
}

void Conductor::SetStreamingState(bool state) {
    std::unique_lock<std::mutex> lock(state_mtx);
    is_ready_for_streaming = state;
    streaming_state.notify_all();
}

bool Conductor::IsReadyForStreaming() const {
    return is_ready_for_streaming;
}

bool Conductor::IsConnected() const {
    return is_connected;
}

/*
// If webrtc can not connect within a certain number of seconds, give up and wait again.
*/
void Conductor::Timeout(int second) {
    sleep(second);
    if (IsReadyForStreaming() && !IsConnected()) {
        SetStreamingState(false);
    }
}

void Conductor::OnRemoteSdp(std::string sdp, std::string sdp_type) {
    absl::optional<webrtc::SdpType> type_maybe =
        webrtc::SdpTypeFromString(sdp_type);
    if (!type_maybe) {
      std::cout << "Unknown SDP type: " << sdp_type << std::endl;
      return;
    }
    webrtc::SdpType type = *type_maybe;

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
        std::cout << "Can't parse received session description message. \n"
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

void Conductor::OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string sdp) {
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, sdp, &error));
    if (!candidate.get()) {
        std::cout << "Can't parse received candidate message. \n"
                  << error.description.c_str() << std::endl;
        return;
    }

    if (!peer_connection_->AddIceCandidate(candidate.get())) {
        std::cout << "Failed to apply the received candidate" << std::endl;
        return;
    }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
        SetSessionDescription::Create(nullptr, nullptr).get(), desc);

    std::string sdp;
    desc->ToString(&sdp);
    std::string type = webrtc::SdpTypeToString(desc->GetType());
    signaling_service_->AnswerLocalSdp(sdp, type);
}

void Conductor::OnFailure(webrtc::RTCError error) {
    std::cout << ToString(error.type()) << ": " << error.message() << std::endl;
}

Conductor::~Conductor() {
    bg_recorder_->Stop();
    audio_track_ = nullptr;
    video_track_ = nullptr;
    peer_connection_ = nullptr;
    peer_connection_factory_ = nullptr;
    rtc::CleanupSSL();
    std::cout << "=> ~Conductor: destroied" << std::endl;
}
