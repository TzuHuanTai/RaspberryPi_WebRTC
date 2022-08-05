#include "../src/v4l2_capture.h"

#include <fcntl.h>
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
    V4L2Capture capture("/dev/video0");
    capture.SetFormat()
        .SetFps(1)
        .StartCapture();

    int file_num = 20;
    for (int i = 0; i < file_num; i++)
    {
        Buffer buffer = capture.CaptureImage();
        WriteImage(buffer, i);
    }
}