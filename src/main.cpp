/*
#include <stdio.h>

#include <string>

#include "api/jsep.h"

using namespace std;
using namespace webrtc;

int main(int argc, char const *argv[])
{
    SdpParseError error;
    string sdp;
    sdp="v=0\no=- 281981094835562477 2 IN IP4 127.0.0.1\ns=-\nt=0 0\na=group:BUNDLE 0 1 2\n";

    auto ptr = CreateSessionDescription("offer", sdp, &error);

    if (!ptr) {
        printf("CreateSessionDescription return null, error str:%s\n", error.description.c_str());
    } else {
        printf("CreateSessionDescription success!\n");
    }

    return 0;
}
*/
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/ssl_adapter.h>
#include <unistd.h>

#include <iostream>

#include "conductor.h"

int main(int argc, char *argv[]) {
    rtc::InitializeSSL();

    std::string server_url = "http://10.128.112.74:5000/SignalingServer";

    rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(server_url));
    // std::unique_ptr<Conductor> conductor(new Conductor());

    conductor->ConnectToPeer();

    sleep(10);

    rtc::CleanupSSL();

    return 0;
}
