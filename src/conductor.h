#ifndef CONDUCTOR_H_
#define CONDUCTOR_H_

#include "args.h"
#include "data_channel_subject.h"
#include "recorder/background_recorder.h"
#include "signaling/signaling_service.h"

#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>

#include <api/peer_connection_interface.h>
#include <api/video/video_sink_interface.h>

class SetSessionDescription : public webrtc::SetSessionDescriptionObserver {
public:
    typedef std::function<void()> OnSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;

    SetSessionDescription(OnSuccessFunc on_success, OnFailureFunc on_failure)
        : on_success_(std::move(on_success)),
          on_failure_(std::move(on_failure)) {}

    static rtc::scoped_refptr<SetSessionDescription> Create(
        OnSuccessFunc on_success, OnFailureFunc on_failure) {
        return rtc::make_ref_counted<SetSessionDescription>(
            std::move(on_success), std::move(on_failure));
    }

protected:
    void OnSuccess() override {
        std::cout << "=> Set sdp success!" << std::endl;
        auto f = std::move(on_success_);
        if (f) {
            f();
        }
    }
    void OnFailure(webrtc::RTCError error) override {
        std::cout << "=> Set sdp failed! " << error.message() << std::endl;
        auto f = std::move(on_failure_);
        if (f) {
            f(error);
        }
    }

    OnSuccessFunc on_success_;
    OnFailureFunc on_failure_;
};

class Conductor : public webrtc::PeerConnectionObserver,
                  public webrtc::CreateSessionDescriptionObserver,
                  public SignalingMessageObserver {
public:
    static rtc::scoped_refptr<Conductor> Create(Args args);

    std::mutex state_mtx;
    std::condition_variable streaming_state;
    Args args;

    Conductor(Args args);
    ~Conductor();
    typedef std::function<void()> OnSetSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;
    typedef std::function<void(webrtc::SessionDescriptionInterface *desc)> OnCreateSuccessFunc;

    bool CreatePeerConnection();
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    void SetStreamingState(bool state);
    bool IsReadyForStreaming() const;
    bool IsConnected() const;
    void Timeout(int second);

protected:
    // PeerConnectionObserver implementation.
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void OnConnectionChange(
        webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
    void OnFailure(webrtc::RTCError error) override;

    // SignalingMessageObserver implementation.
    void OnRemoteSdp(std::string sdp, std::string type) override;
    void OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) override;

private:
    bool is_connected = false;
    bool is_ready_for_streaming = false;

    bool InitializePeerConnectionFactory();
    void InitializeTracks();
    bool InitializeSignaling();
    bool InitializeRecorder();
    void CreateDataChannel();
    void AddTracks();

    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    std::unique_ptr<BackgroundRecorder> bg_recorder_;
    std::unique_ptr<SignalingService> signaling_service_;
    std::shared_ptr<V4L2Capture> video_caputre_source_;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *custom_video_sink_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<webrtc::RtpSenderInterface> video_sender_;

    std::shared_ptr<DataChannelSubject> data_channel_subject_;
};

#endif // CONDUCTOR_H_
