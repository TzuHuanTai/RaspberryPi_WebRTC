#include "parser.h"

#include <boost/program_options.hpp>
#include <iostream>
#include <string>

namespace bpo = boost::program_options;

void Parser::ParseArgs(int argc, char *argv[], Args &args) {
    bpo::options_description opts("Options");
    opts.add_options()("help,h", "Display the help message")(
        "fps", bpo::value<uint32_t>()->default_value(args.fps), "Set camera frame rate")(
        "width", bpo::value<uint32_t>()->default_value(args.width), "Set camera frame width")(
        "height", bpo::value<uint32_t>()->default_value(args.height), "Set camera frame height")(
        "rotation_angle", bpo::value<uint32_t>()->default_value(args.rotation_angle),
        "Set the rotation angle of the frame")(
        "device", bpo::value<std::string>()->default_value(args.device),
        "Read the specific camera file via V4L2, default is /dev/video0")(
        "use_libcamera", bpo::bool_switch()->default_value(args.use_libcamera),
        "Read YUV420 from the camera via libcamera, the `device` and `v4l2_format` flags will be suspended")(
        "no_audio", bpo::bool_switch()->default_value(args.no_audio),
        "Run without audio source")(
        "uid", bpo::value<std::string>()->default_value(args.uid),
        "Set the unique id to identify the device")(
        "stun_url", bpo::value<std::string>()->default_value(args.stun_url),
        "Stun server, ex: stun:xxx.xxx.xxx")(
        "turn_url", bpo::value<std::string>()->default_value(args.turn_url),
        "Turn server, ex: turn:xxx.xxx.xxx:3478?transport=tcp")(
        "turn_username", bpo::value<std::string>()->default_value(args.turn_username),
        "Turn server username")("turn_password",
                                bpo::value<std::string>()->default_value(args.turn_password),
                                "Turn server password")
#if USE_MQTT_SIGNALING
        ("mqtt_port", bpo::value<uint32_t>()->default_value(args.mqtt_port),
         "Mqtt server port")("mqtt_host", bpo::value<std::string>()->default_value(args.mqtt_host),
                             "Mqtt server host")(
            "mqtt_username", bpo::value<std::string>()->default_value(args.mqtt_username),
            "Mqtt server username")("mqtt_password",
                                    bpo::value<std::string>()->default_value(args.mqtt_password),
                                    "Mqtt server password")
#endif
            ("record_path", bpo::value<std::string>()->default_value(args.record_path),
             "The path to save the recording video files. The recorder will not start if it's "
             "empty")(
                "hw_accel", bpo::bool_switch()->default_value(args.hw_accel),
                "Share DMA buffers between decoder/scaler/encoder, which can decrease cpu usage")(
                "v4l2_format", bpo::value<std::string>()->default_value(args.v4l2_format),
                "Set v4l2 camera capture format to `i420`, `mjpeg`, `h264`. The `h264` can pass "
                "packets into mp4 without encoding to reduce cpu usage."
                "Use `v4l2-ctl -d /dev/videoX --list-formats` can list available format");

    bpo::variables_map vm;
    bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
    bpo::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        exit(1);
    }

    if (vm.count("fps")) {
        args.fps = vm["fps"].as<uint32_t>();
    }

    if (vm.count("width")) {
        args.width = vm["width"].as<uint32_t>();
    }

    if (vm.count("height")) {
        args.height = vm["height"].as<uint32_t>();
    }

    if (vm.count("rotation_angle")) {
        args.rotation_angle = vm["rotation_angle"].as<uint32_t>();
    }

    if (vm.count("device")) {
        args.device = vm["device"].as<std::string>();
    }

    if (vm.count("use_libcamera")) {
        args.use_libcamera = vm["use_libcamera"].as<bool>();
        if (args.use_libcamera) {
            args.format = V4L2_PIX_FMT_YUV420;
        }
    }

    if (vm.count("no_audio")) {
        args.no_audio = vm["no_audio"].as<bool>();
    }

    if (vm.count("uid")) {
        args.uid = vm["uid"].as<std::string>();
    }

    if (!vm["stun_url"].empty() && (vm["stun_url"].as<std::string>()).substr(0, 4) != "stun") {
        std::cout << "Stun url should not be empty and start with \"stun:\"" << std::endl;
        exit(1);
    } else if (vm.count("stun_url")) {
        args.stun_url = vm["stun_url"].as<std::string>();
    }

    if (!(!vm["turn_url"].empty() || (vm["turn_url"].as<std::string>()).substr(0, 4) == "turn")) {
        std::cout << "Turn url should start with \"turn:\"" << std::endl;
        exit(1);
    } else if (vm.count("turn_url")) {
        args.turn_url = vm["turn_url"].as<std::string>();
    }

    if (vm.count("turn_username")) {
        args.turn_username = vm["turn_username"].as<std::string>();
    }

    if (vm.count("turn_password")) {
        args.turn_password = vm["turn_password"].as<std::string>();
    }

#if USE_MQTT_SIGNALING
    if (vm.count("mqtt_port")) {
        args.mqtt_port = vm["mqtt_port"].as<uint32_t>();
    }

    if (vm.count("mqtt_host")) {
        args.mqtt_host = vm["mqtt_host"].as<std::string>();
    }

    if (vm.count("mqtt_username")) {
        args.mqtt_username = vm["mqtt_username"].as<std::string>();
    }

    if (vm.count("mqtt_password")) {
        args.mqtt_password = vm["mqtt_password"].as<std::string>();
    }
#endif

    if (vm.count("record_path") && !vm["record_path"].as<std::string>().empty()) {
        if ((vm["record_path"].as<std::string>()).front() != '/') {
            std::cout << "The file path needs to start with a \"/\" character" << std::endl;
            exit(1);
        }

        if ((vm["record_path"].as<std::string>()).back() != '/') {
            args.record_path = vm["record_path"].as<std::string>() + '/';
        } else {
            args.record_path = vm["record_path"].as<std::string>();
        }
    }

    if (vm.count("hw_accel")) {
        args.hw_accel = vm["hw_accel"].as<bool>();
    }

    if (!args.use_libcamera && vm.count("v4l2_format")) {
        args.v4l2_format = vm["v4l2_format"].as<std::string>();

        if (args.v4l2_format == "mjpeg") {
            args.format = V4L2_PIX_FMT_MJPEG;
            printf("Use mjpeg format source in v4l2\n");
        } else if (args.v4l2_format == "h264") {
            args.format = V4L2_PIX_FMT_H264;
            printf("Use h264 format source in v4l2\n");
        } else {
            args.format = V4L2_PIX_FMT_YUV420;
            printf("Use yuv420(i420) format source in v4l2\n");
        }
    }
}
