#ifndef RTC_PEER_H_
#define RTC_PEER_H_

#include <atomic>
#include <iostream>

#include <api/data_channel_interface.h>
#include <api/peer_connection_interface.h>
#include <api/video/video_sink_interface.h>

#include "args.h"
#include "data_channel_subject.h"
#include "signaling/signaling_service.h"

class SetSessionDescription : public webrtc::SetSessionDescriptionObserver {
  public:
    typedef std::function<void()> OnSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;

    SetSessionDescription(OnSuccessFunc on_success, OnFailureFunc on_failure)
        : on_success_(std::move(on_success)),
          on_failure_(std::move(on_failure)) {}

    static rtc::scoped_refptr<SetSessionDescription> Create(OnSuccessFunc on_success,
                                                            OnFailureFunc on_failure) {
        return rtc::make_ref_counted<SetSessionDescription>(std::move(on_success),
                                                            std::move(on_failure));
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

class RtcPeer : public webrtc::PeerConnectionObserver,
                public webrtc::CreateSessionDescriptionObserver,
                public SignalingMessageObserver {
  public:
    using OnCommand = std::function<void(std::shared_ptr<DataChannelSubject>, std::string)>;

    static rtc::scoped_refptr<RtcPeer> Create();

    RtcPeer();
    ~RtcPeer();
    bool IsConnected() const;
    std::string GetId() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    void SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer);
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer();
    void CreateDataChannel();
    void OnSnapshot(OnCommand func);
    void OnMetadata(OnCommand func);
    void OnRecord(OnCommand func);

  private:
    void SubscribeCommandChannel(CommandType type, OnCommand func);

    // PeerConnectionObserver implementation.
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void
    OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void
    OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

    // SignalingMessageObserver implementation.
    void SetRemoteSdp(const std::string &sdp, const std::string &type) override;
    void SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                      const std::string &candidate) override;

    std::string id_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_complete_;
    std::thread timeout_thread_;
    std::shared_ptr<DataChannelSubject> data_channel_subject_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *custom_video_sink_;
};

#endif
