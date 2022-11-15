CC=clang++
CFLAGS= -DWEBRTC_POSIX -DWEBRTC_LINUX \
-D_GLIBCXX_USE_CXX11_ABI \
-DUSE_CPPRESTSDK \
-I./src \
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
-I/usr/local/include/webrtc/third_party/libyuv/include \
-I/usr/local/include/webrtc/tools/json_schema_compiler
WEBRTC_LINK_LIB=-lX11 -ldl
WEBRTC_STATIC_LIB=/usr/local/lib/libwebrtc.a

SIGNALR_LIB_PATH=-L/usr/local/lib
SIGNALR_HEADER_PATH=-I/usr/local/include
SIGNALR_LIB=-lmicrosoft-signalr -lcpprest -lssl -lcrypto

BPO_LINK_LIB=-lboost_program_options
FFMPEG_LINK_LIBS=-lavformat -lavcodec -lavutil -lswscale -lavdevice

.PHONY: all v4l2_capture.o v4l2m2m_encoder.o v4l2_utils.o recorder.o

all: main

v4l2_capture.o: raw_buffer.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} -c ./src/v4l2_capture.cpp

test_v4l2_capture: v4l2_capture.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} ./test/test_v4l2_capture.cpp v4l2_capture.o raw_buffer.o \
	-o test_v4l2_capture ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB}

signal_server.o:
	$(CC) $(CFLAGS) $(SIGNALR_HEADER_PATH) ${WEBRTC_HEADER_PATH} -c ./src/signal_server.cpp

conductor.o: v4l2_capture.o
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) -c ./src/conductor.cpp

raw_buffer.o:
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) -c ./src/encoder/raw_buffer.cpp

customized_video_encoder_factory.o: raw_buffer.o v4l2m2m_encoder.o
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) -c ./src/customized_video_encoder_factory.cpp

parser.o:
	$(CC) $(CFLAGS) -c ./src/parser.cpp

recorder.o:
	$(CC) $(CFLAGS) -c ./src/recorder.cpp

test_recorder: v4l2_capture.o recorder.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} ./test/test_recorder.cpp recorder.o v4l2_capture.o raw_buffer.o \
	-o test_recorder ${FFMPEG_LINK_LIBS} ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB}

v4l2_utils.o: 
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} -c ./src/v4l2_utils.cpp

v4l2m2m_decoder.o: v4l2_utils.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} -c ./src/decoder/v4l2m2m_decoder.cpp

v4l2m2m_encoder.o: v4l2_utils.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} -c ./src/encoder/v4l2m2m_encoder.cpp

test_v4l2m2m_encoder: v4l2m2m_encoder.o recorder.o raw_buffer.o v4l2_capture.o
	$(CC) $(CFLAGS) ${WEBRTC_HEADER_PATH} ./test/test_v4l2m2m_encoder.cpp \
	v4l2m2m_encoder.o recorder.o v4l2_capture.o raw_buffer.o v4l2_utils.o \
	-o test_v4l2m2m_encoder ${FFMPEG_LINK_LIBS} ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB}

main: conductor.o signal_server.o parser.o customized_video_encoder_factory.o
	$(CC) $(CFLAGS) $(WEBRTC_HEADER_PATH) $(SIGNALR_LIB_PATH) $(SIGNALR_HEADER_PATH) ./src/main.cpp *.o \
	-o main ${WEBRTC_STATIC_LIB} ${WEBRTC_LINK_LIB} $(SIGNALR_LIB) ${BPO_LINK_LIB}

clean: 
	rm -f *.o
