#ifndef ARGS_H_
#define ARGS_H_

#include <string>

struct Args
{
  uint32_t fps = 30;
  uint32_t width = 640;
  uint32_t height = 480;
  uint32_t rotation_angle = 0;
  bool use_i420_src = false;
  bool use_h264_hw_encoder = false;
  std::string device = "/dev/video0";
  std::string stun_url = "stun:stun.l.google.com:19302";
  std::string signaling_url = "http://localhost:5000/SignalingServer";
  std::string file_path = "./";
};

#endif // ARGS_H_
