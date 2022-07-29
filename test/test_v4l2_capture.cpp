#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <unistd.h>

// https://www.twblogs.net/a/5b89a7f32b71775d1ce308c2

struct Buffer
{
    void *start;
    size_t length;
};

Buffer buffer;

// uint8_t *buffer;

int querycap_camera(int fd)
{
    //獲取驅動信息，VIDIOC_QUERCAP
    struct v4l2_capability cap;
    int ret = 0;
    memset(&cap, 0, sizeof(cap));
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0)
    {
        printf("VIDIOC_QUERYCAP failed(%d)\n", ret);
        return -1;
    }
    else
    {
        /*輸出 caoability 信息*/
        printf("Capability Informations:\n");
        printf(" driver: %s\n", cap.driver);
        printf(" card: %s\n", cap.card);
        printf(" bus_info: %s\n", cap.bus_info);
        printf(" version: %08X\n", cap.version);
        printf(" capabilities: %08X\n", cap.capabilities);
    }
    printf("--------------------\n"); //可作爲分割線。
    return ret;
}

void enum_camera_format(int fd)
{
    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("Supported format:\n");
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        printf("\t%d.%s\n", fmtdesc.index + 1, fmtdesc.description);
        fmtdesc.index++;
    }
}

int get_camera_fmt(int fd)
{
    struct v4l2_format fmt;
    //獲取視頻格式
    int ret = 0;
    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (ret < 0)
    {
        printf("VIDIOC_G_FMT failed\n");
    }
    else
    {
        printf("VIDIOC_G_FMT succeed\n");
    }
    // printf stream format
    printf("Stream format informations:\n");
    printf(" type: %d\n", fmt.type); //設置了空格，講究格式。
    printf(" width: %d\n", fmt.fmt.pix.width);
    printf(" height: %d\n", fmt.fmt.pix.height);
    char fmtstr[8]; //定義一個字符數組，存儲格式
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &fmt.fmt.pix.pixelformat, 8);
    printf(" pixelformat: %s\n", fmtstr);
    printf(" field: %d\n", fmt.fmt.pix.field);
    printf(" bytesperline: %d\n", fmt.fmt.pix.bytesperline);
    printf(" sizeimage: %d\n", fmt.fmt.pix.sizeimage);
    printf(" colorspace: %d\n", fmt.fmt.pix.colorspace);
    printf(" priv: %d\n", fmt.fmt.pix.priv);
    printf("--------------------\n");
    return 0;
}

int set_format(int fd)
{
    // struct v4l2_format format = {0};
    // format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // format.fmt.pix.width = 320;
    // format.fmt.pix.height = 240;
    // format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    // format.fmt.pix.field = V4L2_FIELD_NONE;
    // int res = ioctl(fd, VIDIOC_S_FMT, &format);
    // if(res == -1) {
    //     perror("Could not set format");
    // }
    // return res;

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG; // V4L2_PIX_FMT_RGB24
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        perror("Setting Pixel Format");
        return 1;
    }
    char fourcc[5] = {0};
    strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    printf("Selected Camera Mode:\n"
           "  Width: %d\n"
           "  Height: %d\n"
           "  PixFmt: %s\n"
           "  Field: %d\n",
           fmt.fmt.pix.width,
           fmt.fmt.pix.height,
           fourcc,
           fmt.fmt.pix.field);
    printf("--------------------\n");
    return 0;
}

int allocate_buffer(int fd)
{
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        perror("Requesting Buffer");
        return 1;
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0)
    {
        perror("Querying Buffer");
        return 1;
    }

    buffer.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    buffer.length = buf.length;
    printf("Length: %d\nAddress: %p\n", buf.length, buffer.start);
    printf("Image Length: %d\n", buf.bytesused);

    return 0;
}

int capture_image(int fd)
{
    struct v4l2_buffer buf
    {
        0
    };
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0)
    {
        perror("Query Buffer");
        return 1;
    }

    if (ioctl(fd, VIDIOC_STREAMON, &buf.type) < 0)
    {
        perror("Start Capture");
        return 1;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r)
    {
        perror("Waiting for Frame");
        return 1;
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
    {
        perror("Retrieving Frame");
        return 1;
    }

    printf("bytesused: %d\n", buf.bytesused);
    int outfd = open("img.jpg", O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno))
    {
        /* open the existing file with write flag */
        outfd = open("img.jpg", O_WRONLY);
    }

    write(outfd, buffer.start, buf.bytesused);
    close(outfd);

    return 0;
}

int main(int argc, char *argv[])
{
    int fd = open("/dev/video0", O_RDWR);
    // querycap_camera(fd);
    // enum_camera_format(fd);
    // get_camera_fmt(fd);

    set_format(fd);

    allocate_buffer(fd);

    capture_image(fd);

    close(fd);
}