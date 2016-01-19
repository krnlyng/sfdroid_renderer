#ifndef __SF_CONNECTION_H__
#define __SF_CONNECTION_H__

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "sfdroid_defs.h"

class sfconnection_t {
    public:
        sfconnection_t() : fd_pass_socket(-1), fd_client(-1) {}
        int init();
        void deinit();
        int wait_for_client();
        int wait_for_buffer(native_handle_t **handle, buffer_info_t *info, int &timedout);
        void send_status_and_cleanup(native_handle_t **handle, int failed);
        bool have_client();
    private:
        int fd_pass_socket; // listen for surfaceflinger
        int fd_client; // the client (sharebuffer module)
};

#endif

