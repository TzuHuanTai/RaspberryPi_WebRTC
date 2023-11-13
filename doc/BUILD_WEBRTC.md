# Build the [native WebRTC](https://webrtc.github.io/webrtc-org/native-code/development/) lib (`libwebrtc.a`)

## Preparations
* Install some dependent packages
    ```bash
    sudo apt remove libssl-dev
    sudo apt install python pulseaudio libpulse-dev build-essential libncurses5 libx11-dev
    pulseaudio --start
    ```

* Install the Chromium `depot_tools`
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
    git checkout -b local-5790 branch-heads/5790 # for m115(stable) version
    git gc --aggressive
    gclient sync -D --force --reset --with_branch_heads --no-history
    ```
* Download `llvm` (Optional)
    ```bash
    curl -OL https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz
    tar Jxvf clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz
    echo 'export PATH=/home/pi/clang+llvm-16.0.6-aarch64-linux-gnu/bin:$PATH' >> ~/.bashrc
    source ~/.bashrc
    ```
* Update `ninja` in the `depot_tools` (Optional)
    ``` bash
    git clone https://github.com/martine/ninja.git;
    cd ninja;
    ./configure.py --bootstrap;
    mv /home/pi/depot_tools/ninja /home/pi/depot_tools/ninja_org;
    cp /home/pi/ninja/ninja /home/pi/depot_tools/ninja;
    ```
* Update `gn` in the `depot_tools` (Optional)
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
    *note: In contrast to the release version, debug version cause frames to be blocked by the video sender.*

* Extract header files
    ```bash
    rsync -amv --include=*/ --include=*.h --include=*.hpp --exclude=* ./ ./webrtc
    ```

* Use scp or other ways to transfer the headers and `libwebrtc.a` to the Raspberry Pi, then move them to `/usr/local`
    ```bash
    # scp -r ./webrtc pi@192.168.x.x:/home/pi
    # scp ./out/Release64/obj/libwebrtc.a pi@192.168.x.x:/home/pi 
    sudo mv /home/pi/webrtc /usr/local/include
    sudo mv /home/pi/libwebrtc.a /usr/local/lib
    ```