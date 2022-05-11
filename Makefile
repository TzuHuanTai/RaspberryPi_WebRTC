CC=clang++
CFLAGS= -DWEBRTC_POSIX -DWEBRTC_LINUX \
-D_GLIBCXX_USE_CXX11_ABI \
-DUSE_CPPRESTSDK \
-stdlib=libstdc++ -g -pthread \
-Wall -fno-exceptions -fno-strict-aliasing -Wno-unknown-pragmas \
-Wno-unused-const-variable -Wno-unused-parameter -Wno-unused-variable \
-Wno-missing-field-initializers -Wno-unused-local-typedefs \
-Wno-uninitialized -Wno-ignored-attributes \
-pipe -fno-ident -fdata-sections -ffunction-sections -fPIC -fpermissive \
-fdeclspec -fexceptions

WEBRTC_LIB_PATH=-L./webrtc/lib 
WEBRTC_HEADER_PATH=-I./webrtc/include \
-I./webrtc/include/third_party/abseil-cpp \
-I./webrtc/include/tools/json_schema_compiler \
-lX11 -ldl
WEBRTC_STATIC_LIB=./webrtc/lib/libwebrtc.a


SIGNALR_LIB_PATH=-L./SignalR-Client-Cpp/build-release/bin
SIGNALR_HEADER_PATH=-I./SignalR-Client-Cpp/include \
-I./SignalR-Client-Cpp/submodules/vcpkg/installed/arm-linux/include
SIGNALR_LIB=-lmicrosoft-signalr #-lssl -lcrypto
CPPREST_STATIC_LIB=./SignalR-Client-Cpp/submodules/vcpkg/installed/arm-linux/lib/*.a

all: main

signal:
	$(CC) $(CFLAGS) $(SIGNALR_LIB_PATH) $(SIGNALR_HEADER_PATH) $(SIGNALR_LIB) ./src/signal.cpp -o signal ${CPPREST_STATIC_LIB}

signal.o:
	$(CC) $(CFLAGS) $(SIGNALR_LIB_PATH) $(SIGNALR_HEADER_PATH) $(SIGNALR_LIB) -c ./src/signal.cpp  ${CPPREST_STATIC_LIB}

conductor.o: signal.o
	$(CC) $(CFLAGS) $(WEBRTC_LIB_PATH) $(WEBRTC_HEADER_PATH) -c ./src/conductor.cpp

main: conductor.o
	$(CC) $(CFLAGS) $(WEBRTC_LIB_PATH) $(WEBRTC_HEADER_PATH) ./src/main.cpp *.o -o main ${WEBRTC_STATIC_LIB}

clean: 
	rm -f main signal *.o