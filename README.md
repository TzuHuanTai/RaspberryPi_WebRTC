# RaspberryPi_WebRTC

Step of using signalr as the webrtc signaling server

1. build the native webrtc on Ubuntu 20.04 64bit
2. build the SignalR-Client with boringssl
3. create .net signalr server hub
4. use signalr-client to exchange ice/sdp information with webrtc.lib.
5. `ioctl` receive camera frames and send them via `AdaptedVideoTrackSource` in webrtc.lib 

## Environment
* RaspberryPi 3B
* RaspberryPi OS 64bit
* [clang 12+](https://github.com/llvm/llvm-project/releases)
* `boringssl` replace `openssl`

## Summary
* Latency is about 0.2 seconds delay.
* Temperatures up to 60~65°C.
* CPU is ~60% at 1280x720 30fps.
![latency](./doc/latency.jpg)

<hr>

# Build the [native WebRTC](https://webrtc.github.io/webrtc-org/native-code/development/) lib (`libwebrtc.a`)

## Preparations
* Install some dependent packages
    ```bash
    sudo apt remove libssl-dev
    sudo apt install python pulseaudio libpulse-dev build-essential libncurses5 libx11-dev
    pulseaudio --start
    ```

* Clone WebRTC source code
    ```bash
    mkdir webrtc-checkout
    cd ./webrtc-checkout
    fetch --nohooks webrtc
    src/build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
    gclient sync -D
    # git checkout -b local-4896 branch-heads/4896 # for m100(stable) version
    # gclient sync -D --force --reset --with_branch_heads
    ```
* Download llvm
    ```bash
    curl -OL https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.3/clang+llvm-14.0.3-aarch64-linux-gnu.tar.xz
    tar Jxvf clang+llvm-14.0.3-aarch64-linux-gnu.tar.xz
    export PATH=/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu/bin:$PATH
    echo 'export PATH=/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu/bin:$PATH' >> ~/.bashrc
    ```
* Install the Chromium `depot_tools`.
    ``` bash
    sudo apt install git
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    export PATH=/home/pi/depot_tools:$PATH
    echo 'PATH=/home/pi/depot_tools:$PATH' >> ~/.bashrc
    ```
* Replace ninja in the `depot_tools`.
    ``` bash
    git clone https://github.com/martine/ninja.git;
    cd ninja;
    ./configure.py --bootstrap;
    mv /home/pi/depot_tools/ninja /home/pi/depot_tools/ninja_org;
    cp /home/pi/ninja/ninja /home/pi/depot_tools/ninja;
    ```
* Replace gn in the `depot_tools`.
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
    gn gen out/Default64 --args='target_os="linux" target_cpu="arm64" rtc_include_tests=false rtc_use_h264=false use_rtti=true is_component_build=false is_debug=true rtc_build_examples=false use_custom_libcxx=false rtc_use_pipewire=false clang_base_path="/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu" treat_warnings_as_errors=false clang_use_chrome_plugins=false'
    ninja -C out/Default64
    ```
cause    *note: In contrast to the release version, debug version cause frames to be blocked by the video sender.*
* Extract header files
    ```bash
    rsync -amv --include=*/ --include=*.h --include=*.hpp --exclude=* ./ ./include
    ```

# Build the [SignalR-Client-Cpp](https://github.com/aspnet/SignalR-Client-Cpp) lib (`microsoft-signalr.so`)
## Preparations
* `sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev`
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
# Reference
* [Version | WebRTC](https://chromiumdash.appspot.com/branches)
* [Building old revisions | WebRTC](https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/building_old_revisions.md)
* [Using a custom clang binary | WebRTC](https://chromium.googlesource.com/chromium/src/+/master/docs/clang.md#using-a-custom-clang-binary)
