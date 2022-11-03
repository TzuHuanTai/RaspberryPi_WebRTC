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
    ("stun_url", bpo::value<std::string>()->default_value(args.stun_url), "Stun server, ex: stun:xxx.xxx.xxx")
    ("signaling_url", bpo::value<std::string>()->default_value(args.signaling_url), "Signaling server url, ref: Repository > FarmerAPI > Hubs > SignalingServer");

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
}