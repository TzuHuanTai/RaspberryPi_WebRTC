#include "conductor.h"

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/rtc_event_log/rtc_event_log_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/video_codecs/video_decoder_factory.h>
#include <api/video_codecs/video_decoder_factory_template.h>
#include <api/video_codecs/video_decoder_factory_template_dav1d_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_open_h264_adapter.h>
#include <media/engine/webrtc_media_engine.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/include/audio_device_factory.h>
#include <modules/audio_device/linux/audio_device_alsa_linux.h>
#include <modules/audio_device/linux/audio_device_pulse_linux.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <pc/video_track_source_proxy.h>
#include <rtc_base/ssl_adapter.h>

#include "common/logging.h"
#include "common/utils.h"
#include "customized_video_encoder_factory.h"
#include "track/h264_dma_track_source.h"
#include "track/v4l2dma_track_source.h"

std::shared_ptr<Conductor> Conductor::Create(Args args) {
    auto ptr = std::make_shared<Conductor>(args);
    ptr->InitializePeerConnectionFactory();
    ptr->InitializeTracks();
    return ptr;
}

Conductor::Conductor(Args args)
    : args(args),
      peers_idx(0),
      is_ready(false) {}

Args Conductor::config() const { return args; }

std::shared_ptr<PaCapture> Conductor::AudioSource() const { return audio_capture_source_; }

std::shared_ptr<V4L2Capture> Conductor::VideoSource() const { return video_capture_source_; }

void Conductor::InitializeTracks() {
    if (audio_track_ == nullptr) {
        audio_capture_source_ = PaCapture::Create(args);
        auto options = peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());
        audio_track_ = peer_connection_factory_->CreateAudioTrack("audio_track", options.get());
    }

    if (video_track_ == nullptr && !args.device.empty()) {
        video_capture_source_ = V4L2Capture::Create(args);

        video_track_source_ = ([this]() -> rtc::scoped_refptr<ScaleTrackSource> {
            if (video_capture_source_->format() == V4L2_PIX_FMT_H264) {
                return H264DmaTrackSource::Create(video_capture_source_);
            } else if (args.enable_v4l2_dma) {
                return V4l2DmaTrackSource::Create(video_capture_source_);
            } else {
                return ScaleTrackSource::Create(video_capture_source_);
            }
        })();

        auto video_source = webrtc::VideoTrackSourceProxy::Create(
            signaling_thread_.get(), worker_thread_.get(), video_track_source_);
        video_track_ = peer_connection_factory_->CreateVideoTrack(video_source, "video_track");
    }
}

void Conductor::AddTracks(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
    if (!peer_connection->GetSenders().empty()) {
        DEBUG_PRINT("Already add tracks.");
        return;
    }

    std::string stream_id = "test_stream_id";

    if (audio_track_) {
        auto audio_res = peer_connection->AddTrack(audio_track_, {stream_id});
        if (!audio_res.ok()) {
            ERROR_PRINT("Failed to add audio track, %s", audio_res.error().message());
        }
    }

    if (video_track_) {
        auto video_res = peer_connection->AddTrack(video_track_, {stream_id});
        if (!video_res.ok()) {
            ERROR_PRINT("Failed to add video track, %s", video_res.error().message());
        }

        auto video_sender_ = video_res.value();
        webrtc::RtpParameters parameters = video_sender_->GetParameters();
        parameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
        video_sender_->SetParameters(parameters);
    }
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

    peer_ = RtcPeer::Create(args, ++peers_idx);
    auto result = peer_connection_factory_->CreatePeerConnectionOrError(
        config, webrtc::PeerConnectionDependencies(peer_.get()));

    if (!result.ok()) {
        DEBUG_PRINT("Peer connection is failed to create!");
        return false;
    }

    peer_->SetPeer(result.MoveValue());
    peer_->CreateDataChannel();
    peer_->OnSnapshot([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        try {
            std::stringstream ss(msg);
            int num;
            ss >> num;
            int quality = ss.fail() ? 100 : num;

            auto i420buff = video_track_source_->GetI420Frame();
            auto jpg_buffer =
                Utils::ConvertYuvToJpeg(i420buff->DataY(), args.width, args.height, quality);
            datachannel->Send(std::move(jpg_buffer));
           
        } catch (const std::exception &e) {
            ERROR_PRINT("%s", e.what());
        }
    });

    peer_->OnThumbnail([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        if (args.record_path.empty()) {
            return;
        }
        try {
            auto latest_jpg_path = Utils::FindLatestJpg(args.record_path);
            auto binary_data = Utils::ReadFileInBinary(latest_jpg_path);
            auto base64_data = Utils::ToBase64(binary_data);

            DEBUG_PRINT("Send Image: %s", latest_jpg_path.c_str());
            std::string data_uri = "data:image/jpeg;base64," + base64_data;
            datachannel->Send(CommandType::THUMBNAIL, data_uri);
        } catch (const std::exception &e) {
            ERROR_PRINT("%s", e.what());
        }
    });

    peer_->OnRecord([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        OnRecord(datachannel, msg);
    });

    peer_->OnReadyToConnect([this](PeerState state) {
        SetPeerReadyState(state.isReadyToConnect);
    });

    AddTracks(peer_->GetPeer());
    peers_map[peers_idx] = peer_;

    DEBUG_PRINT("Peer connection(%d) is created!", peers_idx);
    return true;
}

void Conductor::OnRecord(std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
    if (args.record_path.empty()) {
        return;
    }
    try {
        auto latest_mp4_path = Utils::FindSecondNewestFile(args.record_path, ".mp4");

        std::ifstream file(latest_mp4_path, std::ios::binary | std::ios::ate);
        if (!file) {
            ERROR_PRINT("Unable to open file: %s", latest_mp4_path.c_str());
            return;
        }

        datachannel->Send(file);

        DEBUG_PRINT("Sent Video: %s", latest_mp4_path.c_str());
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> Conductor::GetPeer() const {
    return peer_->GetPeer();
}

void Conductor::SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj) {
    peer_->SetSink(std::move(video_sink_obj));
}

void Conductor::InitializePeerConnectionFactory() {
    rtc::InitializeSSL();

    network_thread_ = rtc::Thread::CreateWithSocketServer();
    worker_thread_ = rtc::Thread::Create();
    signaling_thread_ = rtc::Thread::Create();

    if (network_thread_->Start()) {
        DEBUG_PRINT("network thread start: success!");
    }
    if (worker_thread_->Start()) {
        DEBUG_PRINT("worker thread start: success!");
    }
    if (signaling_thread_->Start()) {
        DEBUG_PRINT("signaling thread start: success!");
    }

    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = network_thread_.get();
    ;
    dependencies.worker_thread = worker_thread_.get();
    dependencies.signaling_thread = signaling_thread_.get();
    dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    dependencies.call_factory = webrtc::CreateCallFactory();
    dependencies.event_log_factory =
        std::make_unique<webrtc::RtcEventLogFactory>(dependencies.task_queue_factory.get());
    dependencies.trials = std::make_unique<webrtc::FieldTrialBasedConfig>();

    cricket::MediaEngineDependencies media_dependencies;
    media_dependencies.task_queue_factory = dependencies.task_queue_factory.get();

    media_dependencies.adm = webrtc::AudioDeviceModule::Create(
        webrtc::AudioDeviceModule::kLinuxPulseAudio, dependencies.task_queue_factory.get());
    media_dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    media_dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    media_dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();
    media_dependencies.audio_mixer = nullptr;
    media_dependencies.video_encoder_factory = CreateCustomizedVideoEncoderFactory(args);
    media_dependencies.video_decoder_factory = std::make_unique<webrtc::VideoDecoderFactoryTemplate<
        webrtc::OpenH264DecoderTemplateAdapter, webrtc::LibvpxVp8DecoderTemplateAdapter,
        webrtc::LibvpxVp9DecoderTemplateAdapter, webrtc::Dav1dDecoderTemplateAdapter>>();
    media_dependencies.trials = dependencies.trials.get();
    dependencies.media_engine = cricket::CreateMediaEngine(std::move(media_dependencies));

    peer_connection_factory_ = CreateModularPeerConnectionFactory(std::move(dependencies));
}

void Conductor::SetPeerReadyState(bool state) {
    std::unique_lock<std::mutex> lock(state_mtx);
    is_ready = state;
    ready_state.notify_all();
}

bool Conductor::IsReady() const { return is_ready; }

void Conductor::AwaitCompletion() {
    {
        std::unique_lock<std::mutex> lock(state_mtx);
        ready_state.wait(lock, [this] {
            return IsReady();
        });
    }

    {
        std::unique_lock<std::mutex> lock(state_mtx);
        ready_state.wait(lock, [this] {
            return !IsReady();
        });
    }

    RefreshPeerList();
}

void Conductor::RefreshPeerList() {
    auto pm_it = peers_map.begin();
    while (pm_it != peers_map.end()) {
        DEBUG_PRINT("Found peers_map key: %d, value: %d", pm_it->second->GetId(),
                    pm_it->second->IsConnected());

        if (pm_it->second && !pm_it->second->IsConnected()) {
            int id = pm_it->second->GetId();
            pm_it->second.release();
            pm_it->second = nullptr;
            pm_it = peers_map.erase(pm_it);
            DEBUG_PRINT("peers_map(%d) was erased.", id);
        } else {
            ++pm_it;
        }
    }
}

Conductor::~Conductor() {
    audio_track_ = nullptr;
    video_track_ = nullptr;
    video_capture_source_ = nullptr;
    peer_connection_factory_ = nullptr;
    rtc::CleanupSSL();
}
