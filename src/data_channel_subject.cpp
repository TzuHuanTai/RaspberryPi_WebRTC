#include "data_channel_subject.h"

#include <iostream>
#include <memory>

DataChannelSubject::~DataChannelSubject()
{
    UnSubscribe();
    data_channel_->UnregisterObserver();
}

void DataChannelSubject::OnStateChange()
{
    webrtc::DataChannelInterface::DataState state = data_channel_->state();
    std::cout << "=> datachannel OnStateChange: " << webrtc::DataChannelInterface::DataStateString(state) << std::endl;
}

void DataChannelSubject::OnMessage(const webrtc::DataBuffer &buffer)
{
    const uint8_t *data = buffer.data.data<uint8_t>();
    size_t length = buffer.data.size();

    char *msg = new char[length + 1];
    memcpy(msg, data, length);
    msg[length] = 0;

    // Send(msg, length);
    Next(msg);

    delete[] msg;
}

void DataChannelSubject::Next(char *message)
{
    for (auto observer : observers_)
    {
        if (observer->subscribed_func_ != nullptr)
        {
            observer->subscribed_func_(message);
        }
    }
}

std::shared_ptr<Observable> DataChannelSubject::AsObservable()
{
    auto observer = std::make_shared<Observable>();
    observers_.push_back(observer);
    return observer;
}

void DataChannelSubject::UnSubscribe()
{
    auto it = observers_.begin();
    while (it != observers_.end())
    {
        it = observers_.erase(it);
    }
}

void DataChannelSubject::Send(char *data, size_t length)
{
    if (data_channel_->state() != webrtc::DataChannelInterface::kOpen)
    {
        return;
    }

    rtc::CopyOnWriteBuffer buffer(data, length);
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel_->Send(data_buffer);
}

void DataChannelSubject::SetDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
    data_channel_ = data_channel;
    data_channel_->RegisterObserver(this);
}
