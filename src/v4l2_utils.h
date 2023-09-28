#ifndef V4L2_UTILS_
#define V4L2_UTILS_

#include "v4l2_utils.h"

#include <linux/videodev2.h>

#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

struct Buffer {
    void *start;
    unsigned int length;
    unsigned int flags;
    struct v4l2_buffer inner;
    struct v4l2_plane plane;
};

struct BufferGroup {
    int fd;
    int num_buffers;
    std::vector<Buffer> buffers;
    enum v4l2_buf_type type;
    enum v4l2_memory memory;
};

class V4l2Util
{
public:
    static bool IsSinglePlaneVideo(v4l2_capability *cap);
    static bool IsMultiPlaneVideo(v4l2_capability *cap);
    static std::string FourccToString(uint32_t fourcc);
    
    static int OpenDevice(const char *file);
    static void CloseDevice(int fd);
    static bool QueryCapabilities(int fd, v4l2_capability *cap);
    static bool InitBuffer(int fd, BufferGroup *gbuffer, v4l2_buf_type type, v4l2_memory memory);
    static bool DequeueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffers(int fd, BufferGroup *buffer);
    static std::unordered_set<std::string> GetDeviceSupportedFormats(const char *file);
    static bool SubscribeEvent(int fd, uint32_t type);
    static bool SetFps(int fd, v4l2_buf_type type, int fps);
    static bool SetFormat(int fd, BufferGroup *gbuffer, int width, int height, uint32_t pixel_format);
    static bool SetCtrl(int fd, uint32_t id, int32_t value);
    static bool SetExtCtrl(int fd, uint32_t id, int32_t value);
    static bool StreamOn(int fd, v4l2_buf_type type);
    static bool StreamOff(int fd, v4l2_buf_type type);
    static void UnMap(BufferGroup *gbuffer);
    static bool MMap(int fd, BufferGroup *gbuffer);
    static bool AllocateBuffer(int fd, BufferGroup *gbuffer, int num_buffers);
    static bool DeallocateBuffer(int fd, BufferGroup *gbuffer);
};

#endif // V4L2_UTILS_
