#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include <functional>
#include <string>

class SignalingMessageObserver {
  public:
    virtual void OnRemoteSdp(std::string sdp, std::string type) = 0;
    virtual void OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate) = 0;
};

class SignalingService {
  public:
    SignalingService()
        : callback_(nullptr){};
    virtual ~SignalingService(){};

    void ResetCallback(SignalingMessageObserver *callback) { callback_ = callback; };
    void ResetCallback() { callback_ = nullptr; };

    virtual void AnswerLocalSdp(std::string sdp, std::string type) = 0;
    virtual void AnswerLocalIce(std::string sdp_mid, int sdp_mline_index,
                                std::string candidate) = 0;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

  protected:
    SignalingMessageObserver *callback_;
};

#endif
