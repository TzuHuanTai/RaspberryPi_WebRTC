#include "parser.h"

#include <string>
#include <iostream>
#include <boost/program_options.hpp>

namespace bpo = boost::program_options;

void Parser::ParseArgs(int argc, char *argv[], Args &args)
{
    bpo::options_description opts("all options");
    opts.add_options()("help,h", "Please call me directly.")
    ("fps", bpo::value<uint32_t>()->default_value(args.fps), "Set ioctl frame rate")
    ("width", bpo::value<uint32_t>()->default_value(args.width), "Set ioctl frame width")
    ("height", bpo::value<uint32_t>()->default_value(args.height), "Set ioctl frame height")
    ("rotation_angle", bpo::value<uint32_t>()->default_value(args.rotation_angle), "Set the rotation angle of the frame")
    ("device", bpo::value<std::string>()->default_value(args.device), "Set the specific camera file, default is /dev/video0")
    ("stun_url", bpo::value<std::string>()->default_value(args.stun_url), "Stun server, ex: stun:xxx.xxx.xxx")
    ("signaling_url", bpo::value<std::string>()->default_value(args.signaling_url), "Signaling server url, ref: Repository > FarmerAPI > Hubs > SignalingServer")
    ("use_i420_src", bpo::value<bool>()->default_value(args.use_i420_src), "Read raw yuv420p source V4L2_PIX_FMT_YUV420 while capturing, if the camera is supported")
    ("use_h264_hw_encoder", bpo::value<bool>()->default_value(args.use_h264_hw_encoder), "Use h264 v4l2 memory-to-memory encoder");

    bpo::variables_map vm;
    bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
    bpo::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        exit(1);
    }
    
    if (vm.count("fps"))
    {
        args.fps = vm["fps"].as<uint32_t>();
    }
    
    if (vm.count("width"))
    {
        args.width = vm["width"].as<uint32_t>();
    }
    
    if (vm.count("height"))
    {
        args.height = vm["height"].as<uint32_t>();
    }

    if (vm.count("rotation_angle"))
    {
        args.rotation_angle = vm["rotation_angle"].as<uint32_t>();
    }
    
    if (vm.count("device"))
    {
        args.device = vm["device"].as<std::string>();
    }

    if (!vm["stun_url"].empty() && (vm["stun_url"].as<std::string>()).substr(0, 4) != "stun")
    {
        std::cout << "Stun url should not be empty and start with \"stun:\"" << std::endl;
        exit(1);
    }
    else if (vm.count("stun_url"))
    {
        args.stun_url = vm["stun_url"].as<std::string>();
    }
    
    if (!vm["signaling_url"].empty() && (vm["signaling_url"].as<std::string>()).substr(0, 4) != "http")
    {
        std::cout << "Signaling url should not be empty and start with \"http:\"" << std::endl;
        exit(1);
    }
    else if (vm.count("signaling_url"))
    {
        args.signaling_url = vm["signaling_url"].as<std::string>();
    }

    if (vm.count("use_i420_src"))
    {
        args.use_i420_src = vm["use_i420_src"].as<bool>();
    }

    if (vm.count("use_h264_hw_encoder"))
    {
        args.use_h264_hw_encoder = vm["use_h264_hw_encoder"].as<bool>();
    }
}
