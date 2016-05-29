#ifndef __SF_CONNECTION_H__
#define __SF_CONNECTION_H__

#include <thread>
#include <vector>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <system/window.h>

#include "sfdroid_defs.h"

extern gralloc_module_t *gralloc_module;

class sfconnection_t {
    public:
        sfconnection_t() : current_status(0), fd_pass_socket(-1), fd_client(-1), running(false), buffer_done(false), current_buffer(NULL), timeout_count(0), sdl_event(-1), have_focus(true) {}
        int init(uint32_t the_sdl_event);
        void deinit();
        int wait_for_client();
        buffer_info_t *get_current_info();
        ANativeWindowBuffer *get_current_buffer();
        void start_thread();
        void thread_loop();
        void stop_thread();
        void update_timeout();
        bool have_client();
        void notify_buffer_done(int failed);

        void remove_buffers();

        void lost_focus();
        void gained_focus();

    private:
        int wait_for_buffer(int &timedout);
        void send_status_and_cleanup();
        int current_status;

        int fd_pass_socket; // listen for surfaceflinger
        int fd_client; // the client (sharebuffer module)

        std::thread my_thread;
        volatile bool running;
        volatile bool buffer_done;

        buffer_info_t current_info;
        ANativeWindowBuffer *current_buffer;
        unsigned int timeout_count;

        uint32_t sdl_event;
        bool have_focus;

        std::vector<ANativeWindowBuffer*> buffers;
        std::vector<buffer_info_t> buffer_infos;
};

#endif

