# RaspberryPi_WebRTC
 
Using v4l2_m2m hardware encoder or raw h264 camera with WebRTC reduces the CPU usage compared to built-in VP8 software encoding.

Step of using signalr as the webrtc signaling server

1. [Build the native webrtc](#build-the-native-webrtc-lib-libwebrtca) on Ubuntu 20.04 64bit
2. [Build the SignalR-Client](#build-the-signalr-client-cpp-lib-microsoft-signalrso) with boringssl
3. Create .net [signalr server hub](https://github.com/TzuHuanTai/FarmerAPI/blob/master/FarmerAPI/Hubs/SignalingServer.cs)
4. Use signalr-client to exchange ice/sdp information with webrtc.lib.
5. Receive camera frames via `ioctl` and send its to `AdaptedVideoTrackSource`, then `VideoEncoder` encode the frame and callback finish encoding.

## Architecture
![architecture](./doc/architecture.png)

## Environment
* RaspberryPi 3B + Raspberry Pi Camera v1.3
* RaspberryPi OS 64bit
* [clang 12+](https://github.com/llvm/llvm-project/releases)
* `boringssl` replace `openssl`

## Summary
* Latency is about 0.2 seconds delay.
* Temperatures up to 60~65°C.
* CPU is ~60% at 1280x720 30fps.

| Codec | Source format | Resolution | FPS | CPU  |  Latency  | Temperature |
| :---: | :-----------: | :--------: | :-: | :--: | :-------: | :---------: |
|  VP8  |     MJPEG     |  1280x720  |  30 | ~60% | 200~250ms |   60~65°C   |
|  VP8  |     MJPEG     |   640x480  |  60 | ~60% | 190~220ms |             |
|  VP8  |     MJPEG     |   320x240  |  60 | ~30% | 120~140ms |             |
|  H264 |     MJPEG     |  1280x720  |  30 | ~35% | 190~200ms |        |
|  H264 |     MJPEG     |   640x480  |  30 | ~25% | 190~200ms |        |
|  H264 |     MJPEG     |   320x240  |  60 | ~25% | 130~200ms |        |
|  H264 |    YUV420     |  1280x720  |  15 | ~20% | 300~350ms |             |
|  H264 |    YUV420     |   640x480  |  15 | ~20% | 200~220ms |             |
|  H264 |    YUV420     |   320x240  |  30 | ~15% | 190~200ms |             |
|    -  |   **H264**    |  1280x720  |  30 | ~25% | ~250ms |             |

![latency](./doc/latency.jpg)

<hr>

# How to use
## Prepare libs
 * Compile `libwebrtc.a` and `microsoft-signalr.so` as instructions below
 * Install the needed packages
    ```bash
    sudo apt install libboost-program-options-dev libavformat-dev libavcodec-dev libavutil-dev libavdevice-dev libswscale-dev
    ```
* Copy the [nlohmann/json.hpp](https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp) to `/usr/local/include` 

## Compile and run
```bash
mkdir build
cd ./build
cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/clang++
make -j
./pi_webrtc --device=/dev/video0 --fps=30 --width=1280 --height=720 --signaling_url=http://localhost:6080/SignalingServer --v4l2_format=mjpeg --record_path=/home/pi/video/
```

## Run as Linux Service
1. Set up [PulseAudio](https://wiki.archlinux.org/title/PulseAudio)
*  Modify this line in `/etc/pulse/system.pa`
    ```ini
    load-module module-native-protocol-unix auth-anonymous=1
    ```

* `sudo nano /etc/systemd/system/pulseaudio.service`, config sample:
    ```ini
    [Unit]
    Description= Pulseaudio Daemon
    After=rtkit-daemon.service systemd-udevd.service dbus.service

    [Service]
    Type=simple
    ExecStart=/usr/bin/pulseaudio --system --disallow-exit --disallow-module-loading --log-target=journal
    Restart=always
    RestartSec=10
      
    [Install]
    WantedBy=multi-user.target
    ```
* Enable and run the service
    ```bash
    sudo systemctl enable pulseaudio.service
    sudo systemctl start pulseaudio.service
    ```

2. Set up WebRTC program 
* `sudo nano /etc/systemd/system/webrtc.service`, config sample:
    ```ini
    [Unit]
    Description= the webrtc service need signaling server first
    After=systemd-networkd.service farmer-api.service

    [Service]
    Type=simple
    WorkingDirectory=/home/pi/IoT/RaspberryPi_WebRTC/build
    ExecStart=/home/pi/IoT/RaspberryPi_WebRTC/build/pi_webrtc --fps=30 --width=1280 --height=720 --signaling_url=http://localhost:6080/SignalingServer --v4l2_format=h264 --record_path=/home/pi/video/
    Restart=always
    RestartSec=10
      
    [Install]
    WantedBy=multi-user.target
    ```
* Enable and run the service
    ```bash
    sudo systemctl enable webrtc.service
    sudo systemctl start webrtc.service
    ```

# Build the [native WebRTC](https://webrtc.github.io/webrtc-org/native-code/development/) lib (`libwebrtc.a`)

## Preparations
* Install some dependent packages
    ```bash
    sudo apt remove libssl-dev
    sudo apt install python pulseaudio libpulse-dev build-essential libncurses5 libx11-dev
    pulseaudio --start
    ```

* Install the Chromium `depot_tools`.
    ``` bash
    sudo apt install git
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    export PATH=/home/pi/depot_tools:$PATH
    echo 'PATH=/home/pi/depot_tools:$PATH' >> ~/.bashrc
    ```
* Clone WebRTC source code
    ```bash
    mkdir webrtc-checkout
    cd ./webrtc-checkout
    fetch --nohooks webrtc
    src/build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
    gclient sync -D
    # git checkout -b local-5790 branch-heads/5790 # for m115(stable) version
    # git gc --aggressive
    # gclient sync -D --force --reset --with_branch_heads --no-history
    ```
* Download llvm(optional)
    ```bash
    curl -OL https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz
    tar Jxvf clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz
    export PATH=/home/pi/clang+llvm-16.0.6-aarch64-linux-gnu/bin:$PATH
    echo 'export PATH=/home/pi/clang+llvm-16.0.6-aarch64-linux-gnu/bin:$PATH' >> ~/.bashrc
    ```
* Replace ninja in the `depot_tools`.(optional)
    ``` bash
    git clone https://github.com/martine/ninja.git;
    cd ninja;
    ./configure.py --bootstrap;
    mv /home/pi/depot_tools/ninja /home/pi/depot_tools/ninja_org;
    cp /home/pi/ninja/ninja /home/pi/depot_tools/ninja;
    ```
* Replace gn in the `depot_tools`.(optional)
    ``` bash
    git clone https://gn.googlesource.com/gn;
    cd gn;
    sed -i -e "s/-Wl,--icf=all//" build/gen.py;
    python build/gen.py;
    ninja -C out;
    sudo mv /home/pi/webrtc-checkout/src/buildtools/linux64/gn /home/pi/webrtc-checkout/src/buildtools/linux64/gn_org;
    cp /home/pi/gn/out/gn /home/pi/webrtc-checkout/src/buildtools/linux64/gn;
    sudo mv /home/pi/depot_tools/gn /home/pi/depot_tools/gn_org;
    cp /home/pi/gn/out/gn /home/pi/depot_tools/gn;
    ```

## Compile `libwebrtc.a`

* Build
    ``` bash
    gn gen out/Release64 --args='target_os="linux" target_cpu="arm64" rtc_include_tests=false rtc_use_h264=false use_rtti=true is_component_build=false is_debug=false rtc_build_examples=false use_custom_libcxx=false rtc_build_tools=false rtc_use_pipewire=false clang_base_path="/home/pi/clang+llvm-16.0.6-aarch64-linux-gnu" clang_use_chrome_plugins=false'
    ninja -C out/Release64
    ```
cause    *note: In contrast to the release version, debug version cause frames to be blocked by the video sender.*
* Extract header files
    ```bash
    rsync -amv --include=*/ --include=*.h --include=*.hpp --exclude=* ./ ./include
    ```

# Build the [SignalR-Client-Cpp](https://github.com/aspnet/SignalR-Client-Cpp) lib (`microsoft-signalr.so`)
## Preparations
* Install some dependent packages
    ```bash
    sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev
    ```
* Install boringssl
    ```bash
    sudo apt-get install libunwind-dev zip unzip tar
    git clone https://github.com/google/boringssl.git
    cd boringssl
    mkdir build
    cd build
    cmake -GNinja .. -DBUILD_SHARED_LIBS=1
    ninja
    sudo ninja install
    sudo cp -r ../install/include/* /usr/local/include/
    sudo cp ../install/lib/*.a /usr/local/lib/
    sudo cp ../install/lib/*.so /usr/local/lib/
    ```
*  Install [cpprestsdk](https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Linux)
    ```bash
    sudo apt-get install libboost-atomic-dev libboost-thread-dev libboost-system-dev libboost-date-time-dev libboost-regex-dev libboost-filesystem-dev libboost-random-dev libboost-chrono-dev libboost-serialization-dev libwebsocketpp-dev
    git clone https://github.com/Microsoft/cpprestsdk.git casablanca
    cd casablanca
    mkdir build.debug
    cd build.debug
    ```
    because we use boringssl rather than openssl, so need modify `/home/pi/casablanca/Release/cmake/cpprest_find_openssl.cmake:53` from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    to
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and replace `-std=c++11` into `-std=c++14` in `Release/CMakeLists.txt`, then
    ``` bash
    cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
    ```
    then build
    ``` bash
    ninja
    sudo ninja install
    ```

## Compile `microsoft-signalr.so`
1. Clone the source code of SignalR-Client-Cpp
    ```bash
    git clone https://github.com/aspnet/SignalR-Client-Cpp.git
    cd ./SignalR-Client-Cpp/
    git submodule update --init
    ```
2. In `SignalR-Client-Cpp/CMakeLists.txt:60`, replace from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    into
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and in `line 17` replace from `-std=c++11` into `-std=c++14` as well.
3. Build
    ``` bash
    cd ..
    mkdir build-release
    cd ./build-release/
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_CPPRESTSDK=true
    cmake --build . --config Release
    sudo make install
    ```

# Install the [coturn](https://github.com/coturn/coturn)
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
3. Set the port `3478` forwarding on the router
4. Start the service, `sudo systemctl start coturn.service`

# Reference
* [Version | WebRTC](https://chromiumdash.appspot.com/branches)
* [Building old revisions | WebRTC](https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/building_old_revisions.md)
* [Using a custom clang binary | WebRTC](https://chromium.googlesource.com/chromium/src/+/master/docs/clang.md#using-a-custom-clang-binary)
* [Trickle ICE](https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/)
