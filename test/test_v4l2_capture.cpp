#include "capture/v4l2_capture.h"
#include "args.h"

#include <condition_variable>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <unistd.h>

void WriteImage(Buffer buffer, int index)
{
    printf("Dequeue buffer index: %d\n"
           "  bytesused: %d\n",
           index, buffer.length);

    std::string filename = "img" + std::to_string(index) + ".jpg";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno))
    {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, buffer.start, buffer.length);
}

int main(int argc, char *argv[])
{
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int i = 0;
    int images_nb = 10;
    auto capture = V4L2Capture::Create("/dev/video0");

    auto test = [&]() -> bool
    {
        if (i < images_nb){
            capture->CaptureImage();
            WriteImage(capture->GetImage(), ++i);
            return true;
        } else {
            is_finished = true;
            cond_var.notify_all();
            return false;
        }
    };

    (*capture).SetFormat(1280, 720, "mjpeg")
        .SetFps(15)
        .SetRotation(0)
        .SetCaptureFunc(test)
        .StartCapture();
    
    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&]
                  { return is_finished; });
}
