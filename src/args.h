#ifndef CONFIG_H_
#define CONFIG_H_

#include <string>

struct Args
{
  uint32_t fps = 30;
  uint32_t width = 640;
  uint32_t height = 480;
  std::string device = "/dev/video0";
  std::string stun_url = "stun:stun.l.google.com:19302";
  std::string signaling_url = "http://localhost:5000/SignalingServer";
};

#endif // CONFIG_H_
