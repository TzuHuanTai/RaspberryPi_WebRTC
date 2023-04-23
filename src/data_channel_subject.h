#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include "subject_interface.h"

#include <api/data_channel_interface.h>
#include <map>

enum class CommandType {
    CONNECT,
    RECORD,
    STREAM,
    UNKNOWN
};

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
    std::shared_ptr<Observable<char *>> AsObservable(CommandType type);
    void UnSubscribe() override;

    void Send(char *data, size_t length);
    void SetDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

private:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
    std::map<CommandType, std::vector<std::shared_ptr<Observable<char *>>>> observers_map_;
};

#endif
