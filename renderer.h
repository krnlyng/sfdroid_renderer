#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <SDL.h>
#include <GLES/gl.h>

#include "sfdroid_defs.h"

#include <hardware/hardware.h>
#include <hardware/gralloc.h>
#include <system/window.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/glext.h>

#include <wayland-client.h>

class renderer_t {
    public:
        renderer_t() : have_focus(0), window(nullptr), glcontext(nullptr), last_screen(nullptr), egl_surf(EGL_NO_SURFACE), egl_ctx(EGL_NO_CONTEXT), w_egl_window(nullptr), buffer(nullptr), last_pixel_format(0), frames_since_focus_gained(0) { }
        int init();
        int render_buffer(ANativeWindowBuffer *the_buffer, buffer_info_t &info, bool now=false);
        int get_height();
        int get_width();
        void gained_focus();
        void lost_focus();
        uint32_t get_window_id();
        bool is_active();
        void deinit(); 

    private:
        int win_width, win_height;
        GLuint dummy_tex;
        bool have_focus;

        SDL_Window *window;
        SDL_GLContext glcontext;

        int save_screen();
        int dummy_draw();
        int draw_raw(void *data, int width, int height, int pixel_format);
        void *last_screen;

        static EGLNativeDisplayType egl_dpy;
        EGLSurface egl_surf;
        EGLContext egl_ctx;
        struct wl_egl_window *w_egl_window;
        ANativeWindowBuffer *buffer;
        static int (*pfn_eglHybrisWaylandPostBuffer)(EGLNativeWindowType win, void *buffer);
        static int instances;

        int last_pixel_format;

        int frames_since_focus_gained;
};

#endif

