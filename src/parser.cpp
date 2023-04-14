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
    ("file_path", bpo::value<std::string>()->default_value(args.file_path), "The path to save the recording video files")
    ("v4l2_format", bpo::value<std::string>()->default_value(args.v4l2_format), "Set v4l2 input format i420/mjpeg/h264 while capturing, if the camera is supported");

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

    if (!vm["file_path"].empty() && 
        ((vm["file_path"].as<std::string>()).front() != '/' ||
        (vm["file_path"].as<std::string>()).back() != '/'))
    {
        std::cout << "The file path needs to start and end with a \"/\" character" << std::endl;
        exit(1);
    }
    else if (vm.count("file_path"))
    {
        args.file_path = vm["file_path"].as<std::string>();
    }

    if (vm.count("v4l2_format"))
    {
        args.v4l2_format = vm["v4l2_format"].as<std::string>();
    }
}
