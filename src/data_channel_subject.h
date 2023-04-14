#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include "subject_interface.h"

#include <api/data_channel_interface.h>

class DataChannelSubject : public webrtc::DataChannelObserver,
                           public SubjectInterface<char *>
{
public:
    DataChannelSubject(){};
    ~DataChannelSubject();

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;

    // SubjectInterface
    void Next(char *message) override;
    std::shared_ptr<Observable<char *>> AsObservable() override;
    void UnSubscribe() override;

    void Send(char *data, size_t length);
    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};

#endif
