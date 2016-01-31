#ifndef __SFDROID_DEFS_H__
#define __SFDROID_DEFS_H__

#define SFDROID_ROOT "/tmp/sfdroid"
#define SHAREBUFFER_HANDLE_FILE (SFDROID_ROOT "/gralloc_buffer_handle")

// hmmm
#define MAX_NUM_FDS 32
#define MAX_NUM_INTS 32

#define SHAREBUFFER_SOCKET_TIMEOUT_NS 500000

#include <cstdint>

struct buffer_info_t
{
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t pixel_format;
};

#include <hardware/gralloc.h>
#include <hardware/hardware.h>

int recv_native_handle(int fd, native_handle_t **handle, struct buffer_info_t *info);
int send_status(int fd, int failed);

#endif

