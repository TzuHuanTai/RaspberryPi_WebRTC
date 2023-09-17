#ifndef V4L2_UTILS_
#define V4L2_UTILS_

#include "v4l2_utils.h"

#include <linux/videodev2.h>

#include <stdint.h>
#include <string>
#include <unordered_set>

struct Buffer {
    void *start;
    unsigned int length;
    unsigned int flags;
    struct v4l2_buffer inner;
    struct v4l2_plane plane;
};

struct BufferGroup
{
    const char *name;
    unsigned int width;
    unsigned int height;
    int num_buffers;
    Buffer* buffers;
    enum v4l2_buf_type type;
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
    static bool InitBuffer(int fd, BufferGroup *output, BufferGroup *capture);
    static bool DequeueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffer(int fd, v4l2_buffer *buffer);
    static std::unordered_set<std::string> GetDeviceSupportedFormats(const char *file);
    static bool SubscribeEvent(int fd, uint32_t type);
    static bool SetFps(int fd, uint32_t type, int fps);
    static bool SetFormat(int fd, BufferGroup *gbuffer, uint32_t pixel_format);
    static bool SetCtrl(int fd, unsigned int id, signed int value);
    static bool SetExtCtrl(int fd, unsigned int id, signed int value);
    static bool StreamOn(int fd, v4l2_buf_type type);
    static bool StreamOff(int fd, v4l2_buf_type type);
    static void UnMap(struct BufferGroup *gbuffer, int num_buffers);
    static bool MMap(int fd, struct BufferGroup *gbuffer, bool is_enqueued);
    static bool AllocateBuffer(int fd, struct BufferGroup *gbuffer, int num_buffers, bool is_enqueued = true);
};

#endif // V4L2_UTILS_
