#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include <functional>
#include <string>

class SignalingService {
public:
    typedef std::function<void(std::string sdp)> OnRemoteSdpFunc;
    typedef std::function<void(std::string sdp_mid, 
                               int sdp_mline_index, 
                               std::string candidate)> OnRemoteIceFunc;

    SignalingService(OnRemoteSdpFunc on_remote_sdp, OnRemoteIceFunc on_remote_ice);
    virtual ~SignalingService() {};

    virtual void AnswerLocalSdp(std::string sdp) = 0;
    virtual void AnswerLocalIce(std::string sdp_mid,
                                int sdp_mline_index,
                                std::string candidate) = 0;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

protected:
    void OnRemoteSdp(std::string sdp);
    void OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate);

private:
    OnRemoteSdpFunc on_remote_sdp_;
    OnRemoteIceFunc on_remote_ice_;
};

#endif
