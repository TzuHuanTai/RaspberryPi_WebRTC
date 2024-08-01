#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include "common/interface/subject.h"

#include <fstream>
#include <map>
#include <vector>

#include <api/data_channel_interface.h>
#include <nlohmann/json.hpp>

#include "common/utils.h"

using json = nlohmann::json;

enum class CommandType : uint8_t {
    CONNECT,
    SNAPSHOT,
    THUMBNAIL,
    RECORD,
    UNKNOWN
};

struct RtcMessage {
    CommandType type;
    std::string message;
    RtcMessage(CommandType type, std::string message)
        : type(type),
          message(message) {}

    std::string ToString() const {
        json j;
        j["type"] = static_cast<int>(type);
        j["message"] = message;
        return j.dump();
    }
};

class DataChannelSubject : public webrtc::DataChannelObserver,
                           public Subject<std::string> {
  public:
    DataChannelSubject() = default;
    ~DataChannelSubject();

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;

    // Subject
    void Next(std::string message) override;
    std::shared_ptr<Observable<std::string>> AsObservable() override;
    std::shared_ptr<Observable<std::string>> AsObservable(CommandType type);
    void UnSubscribe() override;

    void Send(CommandType type, const uint8_t *data, size_t size);
    void Send(const uint8_t *data, size_t size);
    void Send(Buffer image);
    void Send(std::ifstream &file);
    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
    std::map<CommandType, std::vector<std::shared_ptr<Observable<std::string>>>> observers_map_;
};

#endif
