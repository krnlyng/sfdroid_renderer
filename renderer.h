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
        renderer_t() : window(nullptr), glcontext(nullptr), egl_dpy(EGL_NO_DISPLAY), egl_surf(EGL_NO_SURFACE), egl_ctx(EGL_NO_CONTEXT), w_egl_window(nullptr), buffer(nullptr), pfn_eglHybrisWaylandPostBuffer(nullptr) {}
        int init();
        int render_buffer(ANativeWindowBuffer *the_buffer, buffer_info_t &info);
        int get_height();
        int get_width();
        void deinit();

    private:
        int win_width, win_height;
        GLuint dummy_tex;

        SDL_Window *window;
        SDL_GLContext glcontext;

        EGLNativeDisplayType egl_dpy;
        EGLSurface egl_surf;
        EGLContext egl_ctx;
        struct wl_egl_window *w_egl_window;
        ANativeWindowBuffer *buffer;
        int (*pfn_eglHybrisWaylandPostBuffer)(EGLNativeWindowType win, void *buffer);
};

#endif

