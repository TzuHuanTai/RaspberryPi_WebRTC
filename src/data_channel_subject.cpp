#include "data_channel_subject.h"

#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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
    json jsonObj = json::parse(message);

    std::string jsonStr = jsonObj.dump();
    std::cout << "DataChannel Next() => " << jsonStr << std::endl;

    CommandType type = jsonObj["type"];
    std::string content = jsonObj["message"];
    observers_ = observers_map_[type];
    observers_.insert(
        observers_.end(),
        observers_map_[CommandType::UNKNOWN].begin(),
        observers_map_[CommandType::UNKNOWN].end());

    for (auto observer : observers_)
    {
        if (observer->subscribed_func_ != nullptr)
        {
            observer->subscribed_func_(content.data());
        }
    }
}

std::shared_ptr<Observable<char *>> DataChannelSubject::AsObservable()
{
    auto observer = std::make_shared<Observable<char *>>();
    observers_map_[CommandType::UNKNOWN].push_back(observer);
    return observer;
}

std::shared_ptr<Observable<char *>> DataChannelSubject::AsObservable(CommandType type)
{
    auto observer = std::make_shared<Observable<char *>>();
    observers_map_[type].push_back(observer);
    return observer;
}

void DataChannelSubject::UnSubscribe()
{
    for (auto & [type, observers] : observers_map_)
    {
        observers.clear();
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
