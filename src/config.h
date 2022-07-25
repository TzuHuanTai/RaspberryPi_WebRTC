#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>

struct SignalingConfig {
  std::string signaling_url = "http://127.0.0.1:5000/SignalingServer";
};

#endif //CONFIG_H_