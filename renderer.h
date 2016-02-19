#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <SDL.h>
#include <GLES/gl.h>

#include "sfdroid_defs.h"

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/glext.h>

class renderer_t {
    public:
        renderer_t() : last_pixel_format(-1), last_screen(nullptr), window(nullptr), glcontext(nullptr), gralloc_module(nullptr), egl_dpy(EGL_NO_DISPLAY), egl_surf(EGL_NO_SURFACE), egl_ctx(EGL_NO_CONTEXT), w_egl_window(nullptr), buffer(nullptr), pfn_eglCreateImageKHR(nullptr), pfn_glEGLImageTargetTexture2DOES(nullptr), pfn_eglDestroyImageKHR(nullptr) {}
        int init();
        int render_buffer(native_handle_t *the_buffer, buffer_info_t &info);
        int get_height();
        int get_width();
        int save_screen(int pixel_format);
        int dummy_draw();
        int draw_raw(void *data, int width, int height, int pixel_format);
        void deinit();

    private:
        int win_width, win_height;
        int last_pixel_format;
        void *last_screen;
        GLuint tex, dummy_tex;

        SDL_Window *window;
        SDL_GLContext glcontext;

        gralloc_module_t *gralloc_module;

        EGLNativeDisplayType egl_dpy;
        EGLSurface egl_surf;
        EGLContext egl_ctx;
        struct wl_egl_window *w_egl_window;
        ANativeWindowBuffer *buffer = NULL;
        PFNEGLCREATEIMAGEKHRPROC pfn_eglCreateImageKHR;
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pfn_glEGLImageTargetTexture2DOES;
        PFNEGLDESTROYIMAGEKHRPROC pfn_eglDestroyImageKHR;
};

#endif

