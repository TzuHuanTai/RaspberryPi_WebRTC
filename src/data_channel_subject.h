#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include "recorder.h"

#include <stdint.h>
#include <vector>

#include <api/data_channel_interface.h>

// template<typename T>

class ISubject;

class Observable
{
public:
    Observable(){};
    ~Observable(){};
    typedef std::function<void(char *)> OnMessageFunc;

    void Subscribe(OnMessageFunc func)
    {
        subscribed_func_ = func;
    }

    OnMessageFunc subscribed_func_;
};

class ISubject
{
public:
    virtual ~ISubject(){};
    virtual void Next(char *message) = 0;
    virtual std::shared_ptr<Observable> AsObservable() = 0;
    virtual void UnSubscribe() = 0;

    std::vector<std::shared_ptr<Observable>> observers_;
};

class DataChannelSubject : public webrtc::DataChannelObserver,
                           public ISubject
{
public:
    DataChannelSubject(){};
    ~DataChannelSubject();

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;

    // ISubject
    void Next(char *message) override;
    std::shared_ptr<Observable> AsObservable() override;
    void UnSubscribe() override;

    void Send(char *data, size_t length);
    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};

#endif