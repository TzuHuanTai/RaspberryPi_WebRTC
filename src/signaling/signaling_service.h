#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include "conductor.h"

#include <string>

class SignalingService {
public:
    SignalingService(std::shared_ptr<Conductor> conductor);
    virtual ~SignalingService() {};

protected:
    std::shared_ptr<Conductor> conductor_;
    void OnRemoteSdp(std::string sdp);
    void OnRemoteIce(std::string sdp_mid, int sdp_mline_index, std::string candidate);
    virtual void AnswerLocalSdp(std::string sdp) = 0;
    virtual void AnswerLocalIce(std::string sdp_mid,
                                int sdp_mline_index,
                                std::string candidate) = 0;
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

private:
    void InitIceCallback();
};

#endif
