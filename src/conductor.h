#ifndef CONDUCTOR_H_
#define CONDUCTOR_H_

#include "args.h"
#include "recorder/background_recorder.h"
#include "signaling/signaling_service.h"
#include "rtc_peer.h"

#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>

#include <rtc_base/thread.h>
#include <api/peer_connection_interface.h>

class Conductor {
public:
    static std::shared_ptr<Conductor> Create(Args args);

    Conductor(Args args);
    ~Conductor();

    bool CreatePeerConnection();
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    bool IsReady() const;
    void Timeout(int second);
    void BlockUntilSignal();
    void BlockUntilCompletion(int timeout);

private:
    Args args;
    int peers_idx;
    bool is_ready;
    std::mutex state_mtx;
    std::condition_variable ready_state;
    std::unordered_map<int, rtc::scoped_refptr<RtcPeer>> peers_map;

    bool InitializePeerConnectionFactory();
    void InitializeTracks();
    bool InitializeSignaling(Args args);
    bool InitializeRecorder();
    void AddTracks(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
    void RefreshPeerList();
    void SetPeerReadyState(bool state);

    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    std::unique_ptr<BackgroundRecorder> bg_recorder_;
    std::shared_ptr<SignalingService> signaling_service_;
    rtc::scoped_refptr<RtcPeer> peer_;
    std::shared_ptr<V4L2Capture> video_capture_source_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
};

#endif // CONDUCTOR_H_
