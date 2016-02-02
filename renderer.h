#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <SDL.h>
#include <GLES/gl.h>

#include "sfdroid_defs.h"

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

class renderer_t {
    public:
        renderer_t() : window(NULL), glcontext(NULL), gralloc_module(NULL) {}
        int init();
        int render_buffer(native_handle_t *the_buffer, buffer_info_t &info);
        int get_height();
        int get_width();
        int dummy_draw(int pixel_format);
        int draw_raw(void *data, int width, int height, int pixel_format);
        void deinit();

    private:
        int win_width, win_height;
        GLuint tex;

        SDL_Window *window;
        SDL_GLContext glcontext;

        gralloc_module_t *gralloc_module;
};

#endif

