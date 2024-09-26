# Build the project

## Preparation
1. Follow [SETUP_ARM64_ENV](doc/SETUP_ARM64_ENV.md) to prepare an arm64 env for compilation. (Optional)
2. Follow [BUILD_WEBRTC](doc/BUILD_WEBRTC.md) to compile a `libwebrtc.a`.
3. Prepare the MQTT development library.
    * Follow [BUILD_MOSQUITTO](doc/BUILD_MOSQUITTO.md) to compile `mosquitto`.
    * Install the lib from official repo [[tutorial](https://repo.mosquitto.org/debian/README.txt)]. (recommand)
4. Install essential packages
    ```bash
    sudo apt install cmake clang clang-format mosquitto-dev libboost-program-options-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libpulse-dev libasound2-dev libjpeg-dev
    ```
5. Copy the [nlohmann/json.hpp](https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp) to `/usr/local/include`
    ```bash
    sudo mkdir -p /usr/local/include/nlohmann
    sudo curl -L https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -o /usr/local/include/nlohmann/json.hpp
    ```

## Compile and run

| <div style="width:200px">Command line</div> | Default | Valid values |
| --------------------------------------------| ----------- | ------------ |
| -DUSE_MQTT_SIGNALING | ON | (ON, OFF). Build the project by using MOSQUITTO as signaling. |
| -DBUILD_TEST |  | (recorder, mqtt, v4l2_capture, v4l2_encoder, v4l2_decoder, v4l2_scaler). Build the test codes |
| -DCMAKE_BUILD_TYPE | Debug | (Debug, Release) |

Build on raspberry pi and it'll output a `pi_webrtc` file in `/build`.
```bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_BUILD_TYPE=Release
make -j
```

Run `pi_webrtc` to start the service.
```bash
./pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=720 --v4l2_format=mjpeg --mqtt_host=<hostname> --mqtt_port=1883 --mqtt_username=<username> --mqtt_password=<password> --hw_accel
```
