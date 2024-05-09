#ifndef RTC_PEER_H_
#define RTC_PEER_H_

#include "args.h"
#include "data_channel_subject.h"
#include "signaling/signaling_service.h"
#include <api/data_channel_interface.h>
#include <api/peer_connection_interface.h>
#include <api/video/video_sink_interface.h>

#include <iostream>

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

struct PeerState {
    int id;
    bool isConnected;
    bool isReadyToConnect;
};

class RtcPeer : public webrtc::PeerConnectionObserver,
                public webrtc::CreateSessionDescriptionObserver,
                public SignalingMessageObserver,
                public Subject<PeerState> {
public:
    static rtc::scoped_refptr<RtcPeer> Create(Args args, int id);

    RtcPeer(int id);
    ~RtcPeer();
    bool IsConnected() const;
    int GetId() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    void SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer);
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer();
    void SetSignalingService(std::shared_ptr<SignalingService> service);
    void CreateDataChannel();
    void OnReadyToConnect(std::function<void(PeerState)> func);

private:
    void EmitReadyToConnect(bool is_ready);

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

    int id_;
    bool is_connected_;
    bool is_ready_to_connect_;
    std::shared_ptr<SignalingService> signaling_service_;
    std::unique_ptr<DataChannelSubject> data_channel_subject_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *custom_video_sink_;
};

#endif
