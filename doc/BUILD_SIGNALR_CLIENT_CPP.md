# Build the [SignalR-Client-Cpp](https://github.com/aspnet/SignalR-Client-Cpp) lib (`microsoft-signalr.so`)
## Preparations
* Install some dependent packages
    ```bash
    sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev
    ```
* Install the newest golang (Optional)
    ```bash
    sudo apt-get install -y zip unzip tar
    curl -OL https://go.dev/dl/go1.21.4.linux-arm64.tar.gz
    sudo tar -C /usr/local -xzf go1.21.4.linux-arm64.tar.gz
    export GOROOT=/usr/local/go
    export GOPATH=$HOME/go
    export PATH=$GOPATH/bin:$GOROOT/bin:$PATH
    ```

* Install boringssl
    ```bash
    sudo apt-get install libunwind-dev
    git clone https://github.com/google/boringssl.git
    cd boringssl
    git checkout -b chromium-5414 remotes/origin/chromium-5414
    mkdir build
    cd build
    cmake -GNinja ..
    ninja
    sudo ninja install
    sudo cp -r ../install/include/* /usr/local/include/
    sudo cp ../install/lib/*.a /usr/local/lib/
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
    cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=0 -DWERROR=0
    ```
    then build
    ``` bash
    ninja
    sudo ninja install
    ```
* Install Jsoncpp
    ```bash
    git clone https://github.com/open-source-parsers/jsoncpp.git
    cd ./jsoncpp
    mkdir -p build/debug
    cd build/debug
    cmake -DCMAKE_BUILD_TYPE=release -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_INCLUDEDIR=include/jsoncpp -DARCHIVE_INSTALL_DIR=. -G "Unix Makefiles" ../..
    make
    sudo make install
    sudo ln -s /usr/include/jsoncpp/json/ /usr/include/json
    sudo ln -s /usr/local/lib/libjsoncpp.a /usr/local/lib/libjsoncpp_static.a
    ```

## Compile `microsoft-signalr.a`
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
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_CPPRESTSDK=true -DBUILD_SHARED_LIBS=0
    cmake --build . --config Release
    sudo make install
    ```

4. * Use scp or other ways to transfer headers and libs to the Raspberry Pi, then move them to `/usr/local`
    ```bash
    scp -r /usr/local/include/cpprest pi@192.168.x.x:/home/pi
    scp -r /usr/local/include/jsoncpp pi@192.168.x.x:/home/pi
    scp -r /usr/local/include/signalrclient pi@192.168.x.x:/home/pi
    scp /usr/local/lib/libcpprest.a pi@192.168.x.x:/home/pi
    scp /usr/local/lib/libjsoncpp.a pi@192.168.x.x:/home/pi 
    scp /usr/local/lib/libmicrosoft-signalr.a pi@192.168.x.x:/home/pi
    sudo mv /home/pi/cpprest /usr/local/include
    sudo mv /home/pi/jsoncpp /usr/local/include
    sudo mv /home/pi/signalrclient /usr/local/include
    sudo mv /home/pi/*.a /usr/local/lib
    ```

