#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include "common/interface/subject.h"

#include <api/data_channel_interface.h>
#include <map>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

enum class CommandType {
    CONNECT,
    RECORD,
    THUMBNAIL,
    UNKNOWN
};

struct RtcMessage {
    CommandType type;
    std::string message;
    RtcMessage(CommandType type, std::string message)
        : type(type), message(message) {}
    
    std::string ToString() const {
        json j;
        j["type"] = static_cast<int>(type);
        j["message"] = message;
        return j.dump();
    }
};

class DataChannelSubject : public webrtc::DataChannelObserver,
                           public Subject<char*> {
public:
    DataChannelSubject(){};
    ~DataChannelSubject();

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;

    // Subject
    void Next(char *message) override;
    std::shared_ptr<Observable<char*>> AsObservable() override;
    std::shared_ptr<Observable<char*>> AsObservable(CommandType type);
    void UnSubscribe() override;

    void Send(CommandType type, const std::string data);
    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
    std::map<CommandType, std::vector<std::shared_ptr<Observable<char*>>>> observers_map_;
};

#endif
