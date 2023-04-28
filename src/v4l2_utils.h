#ifndef V4L2_UTILS_
#define V4L2_UTILS_

#include "v4l2_utils.h"

#include <linux/videodev2.h>

#include <stdint.h>
#include <string>
#include <unordered_set>

struct Buffer
{
    const char *name;
    void *start;
    unsigned int length;
    unsigned int width;
    unsigned int height;
    unsigned int flags;
    enum v4l2_buf_type type;
    struct v4l2_buffer inner;
    struct v4l2_plane plane;
};

class V4l2Util
{
public:
    static bool IsSinglePlaneVideo(struct v4l2_capability *cap);
    static bool IsMultiPlaneVideo(struct v4l2_capability *cap);
    static std::string FourccToString(uint32_t fourcc);
    
    static int OpenDevice(const char *file);
    static void CloseDevice(int fd);
    static bool QueryCapabilities(int fd, v4l2_capability *cap);
    static bool InitBuffer(int fd, Buffer *output, Buffer *capture);
    static std::unordered_set<std::string> GetDeviceSupportedFormats(const char *file);
    static bool SetFps(int fd, uint32_t type, int fps);
    static bool SetFormat(int fd, Buffer *buffer, uint32_t pixel_format);
    static bool SetCtrl(int fd, unsigned int id, signed int value);
    static bool SetExtCtrl(int fd, unsigned int id, signed int value);
    static bool StreamOn(int fd, v4l2_buf_type type);
    static bool StreamOff(int fd, v4l2_buf_type type);
    static bool MMap(int fd, struct Buffer *buffer);
    static bool AllocateBuffer(int fd, struct Buffer *buffer);
};

#endif // V4L2_UTILS_
