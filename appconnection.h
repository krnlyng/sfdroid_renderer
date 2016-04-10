#ifndef __APP_CONNECTION_H__
#define __APP_CONNECTION_H__

#include "sfdroid_defs.h"

#include <thread>
#include <atomic>
#include <map>

#include <SDL.h>

#include "renderer.h"

class appconnection_t {
    public:
        appconnection_t(renderer_t &renderer, std::map<std::string, renderer_t*> &windows) : fd_socket(-1), fd_client(-1), running(false), have_focus(true), my_renderer(renderer), my_windows(windows) {}
        int init(uint32_t the_sdl_evenv);
        void deinit();
        int wait_for_client();
        bool have_client();
        void update_timeout();
        int wait_for_request(int &type, int &timedout, std::string &app);
        void start_thread();
        void thread_loop();
        void stop_thread();

        void lost_focus();
        void gained_focus();

        std::string get_new_window();
    private:
        int fd_socket; // listen for sfdroid_Helpers
        int fd_client; // the client (sfdroid_Helpers)

        std::thread my_thread;

        std::atomic<bool> running;

        bool have_focus;

        uint32_t sdl_event;
        renderer_t &my_renderer;
        std::map<std::string, renderer_t*> &my_windows;
        std::string new_window;
};

#endif

