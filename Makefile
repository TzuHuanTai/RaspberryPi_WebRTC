CC=clang++
CFLAGS= -DWEBRTC_POSIX -DWEBRTC_LINUX \
-D_GLIBCXX_USE_CXX11_ABI \
-DUSE_CPPRESTSDK \
-std=c++17 -g -pthread \
-Wall -fno-exceptions -fno-strict-aliasing -Wno-unknown-pragmas \
-Wno-unused-const-variable -Wno-unused-parameter -Wno-unused-variable \
-Wno-missing-field-initializers -Wno-unused-local-typedefs \
-Wno-uninitialized -Wno-ignored-attributes \
-pipe -fno-ident -fdata-sections -ffunction-sections -fPIC -fpermissive \
-fdeclspec -fexceptions

WEBRTC_LIB_PATH=-L/usr/local/lib
WEBRTC_HEADER_PATH=-I/usr/local/include/webrtc \
-I/usr/local/include/webrtc/third_party/abseil-cpp \
-I/usr/local/include/webrtc/tools/json_schema_compiler
WEBRTC_LINK_LIB=-lX11 -ldl
WEBRTC_STATIC_LIB=/usr/local/lib/libwebrtc.a


SIGNALR_LIB_PATH=-L/usr/local/lib
SIGNALR_HEADER_PATH=-I/usr/local/include
SIGNALR_LIB=-lmicrosoft-signalr -lcpprest -lssl -lcrypto

.PHONY: all signal.o conductor.o main v4l2_capture.o test_v4l2_capture

all: main

v4l2_capture.o: 
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} -c ./src/v4l2_capture.cpp

test_v4l2_capture: v4l2_capture.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} ./test/test_v4l2_capture.cpp v4l2_capture.o -o test_v4l2_capture  ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB}

signal:
	$(CC) $(CFLAGS) $(SIGNALR_LIB_PATH) $(SIGNALR_HEADER_PATH) ${WEBRTC_HEADER_PATH} $(SIGNALR_LIB) ./src/signal.cpp -o signal

signal.o:
	$(CC) $(CFLAGS) $(SIGNALR_HEADER_PATH) ${WEBRTC_HEADER_PATH} -c ./src/signal.cpp

conductor.o:
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) -c ./src/conductor.cpp

main: conductor.o signal.o
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) $(SIGNALR_LIB_PATH) $(SIGNALR_HEADER_PATH) $(SIGNALR_LIB) ./src/main.cpp *.o -o main ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB}

clean: 
	rm -f main signal *.o