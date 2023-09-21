#ifndef ARGS_H_
#define ARGS_H_

#include <string>

struct Args
{
  int fps = 30;
  int width = 640;
  int height = 480;
  int rotation_angle = 0;
  std::string v4l2_format = "mjpeg";
  std::string device = "/dev/video0";
  std::string stun_url = "stun:stun.l.google.com:19302";
  std::string turn_url = "";
  std::string turn_username = "";
  std::string turn_password = "";
  std::string signaling_url = "http://localhost:5000/SignalingServer";
  std::string record_path = "";
  std::string record_container = "mp4";
  std::string encoder_name = "h264_v4l2m2m";
};

#endif // ARGS_H_
