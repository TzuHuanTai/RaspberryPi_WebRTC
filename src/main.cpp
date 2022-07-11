#include <iostream>
#include <string>
#include <unistd.h>

#include <rtc_base/ref_counted_object.h>

#include "conductor.h"
#include "config.h"

int main(int argc, char *argv[])
{
    SignalingConfig config;
    
    rtc::scoped_refptr<Conductor> conductor = rtc::make_ref_counted<Conductor>(config.signaling_url);

    //conductor->ConnectToPeer();

    sleep(10);

    return 0;
}
