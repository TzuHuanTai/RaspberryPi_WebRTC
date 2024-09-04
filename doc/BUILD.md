# Build the project

## Preparation
1. Follow [SETUP_ARM64_ENV](doc/SETUP_ARM64_ENV.md) to prepare an arm64 env for compilation (Optional)
2. Follow [BUILD_WEBRTC](doc/BUILD_WEBRTC.md) to compile `libwebrtc.a` 
3. Choose a signaling mechanisum
    * Follow [BUILD_MOSQUITTO](doc/BUILD_MOSQUITTO.md) to compile `mosquitto` (recommand)
    * Follow [BUILD_SIGNALR_CLIENT_CPP](doc/BUILD_SIGNALR_CLIENT_CPP.md) to compile `microsoft-signalr`
4. Install `FFmpeg` and the needed packages on pi
    ```bash
    sudo apt install libboost-program-options-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libpulse-dev libasound2-dev libjpeg-dev
    ```
5. Copy the [nlohmann/json.hpp](https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp) to `/usr/local/include` 
6. Run the chosen signaling server
    * Create a [signalr server hub](https://github.com/TzuHuanTai/FarmerAPI/blob/master/FarmerAPI/Hubs/SignalingServer.cs) on .net
    * Follow [SETUP_MOSQUITTO](doc/SETUP_MOSQUITTO.md)

## Compile and run

| <div style="width:200px">Command line</div> | Description | Valid values |
| --------------------------------------------| ----------- | ------------ |
|   -DUSE_SIGNALR_SIGNALING   | Build the project by using SignalR as signaling. | ON, OFF |
|   -DUSE_MQTT_SIGNALING      | Build the project by using MOSQUITTO as signaling. | ON, OFF |
|   -DBUILD_TEST              | Build the test codes | recorder, mqtt, v4l2_capture, v4l2_encoder, v4l2_decoder, v4l2_scaler |

Build on raspberry pi and it'll output a `pi_webrtc` file in `/build`.
```bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DUSE_MQTT_SIGNALING=ON
make -j
```

Run `pi_webrtc` to start the service.

* If use signalr as signaling.
    ```bash
    ./pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=720 --v4l2_format=mjpeg --signaling_url=http://localhost:6080/SignalingServer --hw_accel
    ```
* If use mosquitto mqtt as signaling.
    ```bash
    ./pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=720 --v4l2_format=mjpeg --mqtt_host=127.0.0.1 --mqtt_port=1883 --mqtt_username=<username> --mqtt_password=<password>  --hw_accel
    ```
* `./pi_webrtc -h` to list all available args.
* The `VP8`, `VP9` are available only if the source format is `mjpeg` or `i420` not be assigned, and frames will be decoded/scaled by software, the buffer will be copied between HW encoder and user space.
* If the `--record_path` is assigned, the background recorder will start immediately after running the program. But the performance of Pi 3B is limited, if the `--v4l2_format=mjpeg` source resolution is above 640x368@15fps the HW codec will be unstable, stuck, or even crash. However, it can record smoothly at 960x480@30fps when the `--v4l2_format=h264` and `--hw_accel` flags are applied, even with two clients watching p2p streams simultaneously. Since the h264 source directly records into mp4 files without going through the decode/encode process.

# Install the [coturn](https://github.com/coturn/coturn) (Optional)
If the cellular network is used, the `coturn` is required because the 5G NAT setting by ISP may block p2p. Or try some cloud service that provides TURN server.
1. Install
    ```bash
    sudo apt update
    sudo apt install coturn
    sudo systemctl stop coturn.service
    ```
2. Edit config `sudo nano /etc/turnserver.conf`, uncomment or modify below options
    ```ini
    listening-port=3478
    listening-ip=192.168.x.x
    relay-ip=192.168.x.x
    external-ip=174.127.x.x/192.168.x.x
    #verbose
    lt-cred-mech
    user=webrtc:webrtc
    realm=greenhouse
    no-tls
    no-dtls
    syslog
    no-cli
    ```
3. Set the port `3478` forwarding on the router/modem.
4. Start the service, `sudo systemctl start coturn.service`

# Reference
* [Version | WebRTC](https://chromiumdash.appspot.com/branches)
* [Building old revisions | WebRTC](https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/building_old_revisions.md)
* [Using a custom clang binary | WebRTC](https://chromium.googlesource.com/chromium/src/+/master/docs/clang.md#using-a-custom-clang-binary)
* [Trickle ICE](https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/)
