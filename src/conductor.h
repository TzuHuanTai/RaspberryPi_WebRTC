/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CONDUCTOR_H_
#define CONDUCTOR_H_

#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include <api/peer_connection_interface.h>
#include <examples/peerconnection/client/main_wnd.h>
#include <examples/peerconnection/client/peer_connection_client.h>

namespace webrtc
{
    class VideoCaptureModule;
} // namespace webrtc

namespace cricket
{
    class VideoRenderer;
} // namespace cricket

class SetSessionDescription : public webrtc::SetSessionDescriptionObserver
{
public:
    typedef std::function<void()> OnSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;

    SetSessionDescription(OnSuccessFunc on_success, OnFailureFunc on_failure)
        : on_success_(std::move(on_success)),
          on_failure_(std::move(on_failure)){}

    static SetSessionDescription *Create(OnSuccessFunc on_success, OnFailureFunc on_failure)
    {
        return new rtc::RefCountedObject<SetSessionDescription>(std::move(on_success), std::move(on_failure));
    }

protected:
    void OnSuccess() override
    {
        std::cout << "=> Set OnSuccess: " << std::endl;
        auto f = std::move(on_success_);
        if (f) {
            f();
        }
    }
    void OnFailure(webrtc::RTCError error) override
    {
        std::cout << "=> Set OnFailure: " << error.message() << std::endl;
        auto f = std::move(on_failure_);
        if (f) {
            f(error);
        }
    }

    OnSuccessFunc on_success_;
    OnFailureFunc on_failure_;
};

class CreateSessionDescription : public webrtc::CreateSessionDescriptionObserver
{
public:
    typedef std::function<void(webrtc::SessionDescriptionInterface *desc)> OnSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;

    CreateSessionDescription(OnSuccessFunc on_success, OnFailureFunc on_failure)
        : on_success_(std::move(on_success)),
          on_failure_(std::move(on_failure)){}

    static CreateSessionDescription *Create(OnSuccessFunc on_success, OnFailureFunc on_failure)
    {
        return new rtc::RefCountedObject<CreateSessionDescription>(std::move(on_success), std::move(on_failure));
    }

protected:
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override
    {
        std::cout << "=> Set OnSuccess: " << std::endl;
        auto f = std::move(on_success_);
        if (f) {
            f(desc);
        }
    }
    void OnFailure(webrtc::RTCError error) override
    {
        std::cout << "=> Set OnFailure: " << error.message() << std::endl;
        auto f = std::move(on_failure_);
        if (f) {
            f(error);
        }
    }

    OnSuccessFunc on_success_;
    OnFailureFunc on_failure_;
};

class Conductor : public webrtc::PeerConnectionObserver
{
public:
    std::string signalr_url;
    enum CallbackID
    {
        MEDIA_CHANNELS_INITIALIZED = 1,
        PEER_CONNECTION_CLOSED,
        SEND_MESSAGE_TO_PEER,
        NEW_TRACK_ADDED,
        TRACK_REMOVED,
    };

    Conductor(std::string server_url);
    ~Conductor();
    void ConnectToPeer();
    typedef std::function<void()> OnSetSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;
    typedef std::function<void(webrtc::SessionDescriptionInterface *desc)> OnCreateSuccessFunc;

    typedef std::function<void(std::string)> InvokeSdpFunc;
    typedef std::function<void(std::string, int, std::string)> InvokeIceFunc;
    InvokeSdpFunc invoke_answer_sdp_;
    InvokeIceFunc invoke_answer_ice_;

    void SetOfferSDP(const std::string sdp,
                              OnSetSuccessFunc on_success,
                              OnFailureFunc on_failure);
    void AddIceCandidate(std::string sdp_mid, int sdp_mline_index, std::string candidate);
    void CreateAnswer(OnCreateSuccessFunc on_success, OnFailureFunc on_failure);
    void SendData(const std::string msg);
    // bool connection_active() const;

protected:
    bool InitializePeerConnection();
    // bool ReinitializePeerConnectionForLoopback();
    bool CreatePeerConnection();
    // void DeletePeerConnection();
    // void EnsureStreamingUI();
    void AddTracks();

    //
    // PeerConnectionObserver implementation.
    //

    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    // void OnAddTrack(
    //     rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    //     const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
    // void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    // void OnRenegotiationNeeded() override {}
    void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    // void OnIceConnectionReceivingChange(bool receiving) override {}
    void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

    //
    // PeerConnectionClientObserver implementation.
    //

    // void OnSignedIn() override;

    // void OnDisconnected() override;

    // void OnPeerConnected(int id, const std::string& name) override;

    // void OnPeerDisconnected(int id) override;

    // void OnMessageFromPeer(int peer_id, const std::string& message) override;

    // void OnMessageSent(int err) override;

    // void OnServerConnectionFailure() override;

    // CreateSessionDescriptionObserver implementation.

protected:
    // Send a message to the remote peer.
    // void SendMessage(const std::string& json_object);

    int peer_id_;
    bool loopback_;
    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel_;
    PeerConnectionClient *client_;
    std::deque<std::string *> pending_messages_;
};

#endif // CONDUCTOR_H_
