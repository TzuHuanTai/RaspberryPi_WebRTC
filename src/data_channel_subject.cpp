#include "data_channel_subject.h"
#include "common/logging.h"

#include <iostream>
#include <memory>

DataChannelSubject::~DataChannelSubject() {
    UnSubscribe();
    data_channel_->UnregisterObserver();
    data_channel_->Close();
}

void DataChannelSubject::OnStateChange() {
    webrtc::DataChannelInterface::DataState state = data_channel_->state();
    DEBUG_PRINT("OnStateChange => %s", webrtc::DataChannelInterface::DataStateString(state));
}

void DataChannelSubject::OnMessage(const webrtc::DataBuffer &buffer) {
    const uint8_t *data = buffer.data.data<uint8_t>();
    size_t length = buffer.data.size();
    std::string message(reinterpret_cast<const char*>(data), length);

    Next(message);
}

void DataChannelSubject::Next(std::string &message) {
    try {
        json jsonObj = json::parse(message.c_str());

        std::string jsonStr = jsonObj.dump();
        DEBUG_PRINT("Receive message => %s", jsonStr.c_str());

        CommandType type = jsonObj["type"];
        std::string content = jsonObj["message"];
        if (content.empty()) {
            return;
        }
        observers_ = observers_map_[type];
        observers_.insert(
            observers_.end(),
            observers_map_[CommandType::UNKNOWN].begin(),
            observers_map_[CommandType::UNKNOWN].end());

        for (auto observer : observers_) {
            if (observer->subscribed_func_ != nullptr) {
                observer->subscribed_func_(content);
            }
        }
    } catch(const json::parse_error& e) {
        ERROR_PRINT("JSON parse error, %s, occur at position: %lu", e.what(), e.byte);
    }
}

std::shared_ptr<Observable<std::string>> DataChannelSubject::AsObservable() {
    auto observer = std::make_shared<Observable<std::string>>();
    observers_map_[CommandType::UNKNOWN].push_back(observer);
    return observer;
}

std::shared_ptr<Observable<std::string>> DataChannelSubject::AsObservable(CommandType type) {
    auto observer = std::make_shared<Observable<std::string>>();
    observers_map_[type].push_back(observer);
    return observer;
}

void DataChannelSubject::UnSubscribe() {
    for (auto & [type, observers] : observers_map_)
    {
        observers.clear();
    }
}

void DataChannelSubject::Send(CommandType type, const std::string data) {
    if (data_channel_->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }
    RtcMessage msg(type, data);
    webrtc::DataBuffer data_buffer(msg.ToString());
    data_channel_->Send(data_buffer);
}

void DataChannelSubject::Send(const uint8_t* data, size_t size) {
    if (data_channel_->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }
    rtc::CopyOnWriteBuffer buffer(data, size);
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel_->Send(data_buffer);
}

void DataChannelSubject::SetDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    data_channel_ = data_channel;
    data_channel_->RegisterObserver(this);
}
