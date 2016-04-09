/*
 *  this file is part of sfdroid
 *  Copyright (C) 2015, Franz-Josef Haider <f_haider@gmx.at>
 *  based on harmattandroid by Thomas Perl
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "renderer.h"

#include <iostream>

#include <GLES/gl.h>

#include <SDL_syswm.h>
#include <wayland-egl.h>

#include "sfconnection.h"

using namespace std;

EGLNativeDisplayType renderer_t::egl_dpy(EGL_NO_DISPLAY);
int (*renderer_t::pfn_eglHybrisWaylandPostBuffer)(EGLNativeWindowType win, void *buffer)(nullptr);
int renderer_t::instances(0);

int renderer_t::init()
{
    int err = 0;
    char windowname[128];

    SDL_SysWMinfo info;
    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };
    EGLConfig egl_cfg;
    EGLint contextParams[] = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE};
    EGLint numConfigs;

    instances++;

#if DEBUG
    cout << "creating SDL window" << endl;
#endif

    snprintf(windowname, 128, "sfdroid%d", instances);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    window = SDL_CreateWindow(windowname, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if(window == NULL)
    {
        cerr << "failed to create SDL window" << endl;
        err = 2;
        goto quit;
    }

    SDL_GetWindowSize(window, &win_width, &win_height);

#if DEBUG
    cout << "window width: " << win_width << " height: " << win_height << endl;
#endif

    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    if(egl_dpy == EGL_NO_DISPLAY)
    {
#if DEBUG
        cout << "getting egl display" << endl;
#endif
        egl_dpy = eglGetDisplay((EGLNativeDisplayType)info.info.wl.display);
        if(egl_dpy == EGL_NO_DISPLAY)
        {
            cerr << "failed to get egl display" << endl;
            err = 5;
            goto quit;
        }

#if DEBUG
        cout << "initializing egl display" << endl;
#endif
        if(!eglInitialize(egl_dpy, NULL, NULL))
        {
            cerr << "failed to initialize egl display" << endl;
            err = 6;
            goto quit;
        }
    }

#if DEBUG
    cout << "choosing egl config" << endl;
#endif
    if(eglChooseConfig(egl_dpy, configAttribs, &egl_cfg, 1, &numConfigs) != EGL_TRUE || numConfigs == 0)
    {
        cerr << "unable to find an EGL Config" << endl;
        err = 7;
        goto quit;
    }

#if DEBUG
    cout << "creating wl egl window" << endl;
#endif
    w_egl_window = wl_egl_window_create (info.info.wl.surface, win_width, win_height);

#if DEBUG
    cout << "creating egl window surface" << endl;
#endif
    egl_surf = eglCreateWindowSurface(egl_dpy, egl_cfg, (EGLNativeWindowType)w_egl_window, 0);
    if(egl_surf == EGL_NO_SURFACE)
    {
        cerr << "unable to create an EGLSurface" << endl;
        err = 8;
        goto quit;
    }

#if DEBUG
    cout << "creating GLES context" << endl;
#endif
    eglBindAPI(EGL_OPENGL_ES_API);
    egl_ctx = eglCreateContext(egl_dpy, egl_cfg, NULL, contextParams);
    if(egl_ctx == EGL_NO_CONTEXT)
    {
        cerr << "unable to create GLES context" << endl;
        err = 9;
        goto quit;
    }

#if DEBUG
    cout << "making GLES context current" << endl;
#endif
    if(eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx) == EGL_FALSE)
    {
        cerr << "unable to make GLES context current" << endl;
        err = 10;
        goto quit;
    }

#if DEBUG
    cout << "getting eglHybrisWaylandPostBuffer" << endl;
#endif
    pfn_eglHybrisWaylandPostBuffer = (int (*)(EGLNativeWindowType, void *))eglGetProcAddress("eglHybrisWaylandPostBuffer");
    if(pfn_eglHybrisWaylandPostBuffer == NULL)
    {
        cerr << "eglHybrisWaylandPostBuffer not found" << endl;
        err = 15;
        goto quit;
    }

#if DEBUG
    cout << "setting up gl" << endl;
#endif
    glViewport(0, 0, win_width, win_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, win_width, win_height, 0, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.f, 1.f, 1.f, 1.f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glGenTextures(1, &dummy_tex);
    glBindTexture(GL_TEXTURE_2D, dummy_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

quit:
    return err;
}

void renderer_t::deinit()
{
    glDeleteTextures(1, &dummy_tex);
    eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_dpy, egl_surf);
    wl_egl_window_destroy(w_egl_window);
    eglDestroyContext(egl_dpy, egl_ctx);
    instances--;
    if(instances == 0) eglTerminate(egl_dpy);
    if(window) SDL_DestroyWindow(window);
}

int renderer_t::draw_raw(void *data, int width, int height, int pixel_format)
{
    int err = 0;
    GLuint gl_err = 0;

    float xf = (float)win_width / (float)width;
    float yf = 1.f;
    float texcoords[] = {
        0.f, 0.f,
        xf, 0.f,
        0.f, yf,
        xf, yf,
    };

    float vtxcoords[] = {
        0.f, 0.f,
        (float)win_width, 0.f,
        0.f, (float)win_height,
        (float)win_width, (float)win_height,
    };

    glVertexPointer(2, GL_FLOAT, 0, &vtxcoords);
    glTexCoordPointer(2, GL_FLOAT, 0, &texcoords);

    glBindTexture(GL_TEXTURE_2D, dummy_tex);

    if(buffer->format == HAL_PIXEL_FORMAT_RGBA_8888 || buffer->format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    else if(buffer->format == HAL_PIXEL_FORMAT_RGB_565)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
    }
    else
    {
        cerr << "unhandled pixel format: " << buffer->format << endl;
        err = 3;
        goto quit;
    }
    gl_err = glGetError();
    if(gl_err != GL_NO_ERROR) cout << "glGetError(): " << gl_err << endl;

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(egl_dpy, egl_surf);

quit:
    return err;
}

uint32_t renderer_t::get_window_id()
{
    return SDL_GetWindowID(window);
}

int renderer_t::save_screen()
{
    int err = 0;
    int gerr = 0;
    void *buffer_vaddr;

    if(buffer == nullptr)
    {
        err = 2;
        goto quit;
    }

    gerr = gralloc_module->lock(gralloc_module, buffer->handle,
        GRALLOC_USAGE_SW_READ_RARELY,
        0, 0, buffer->width, buffer->height,
        &buffer_vaddr);

    if(gerr)
    {
        err = 3;
        goto quit;
    }

    if(last_screen) free(last_screen);
    last_screen = nullptr;

    if(buffer->format == HAL_PIXEL_FORMAT_RGBA_8888 || buffer->format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        last_screen = (GLubyte*)malloc(4 * buffer->stride * buffer->height);
        memcpy(last_screen, buffer_vaddr, 4 * buffer->stride * buffer->height);
    }
    else if(buffer->format == HAL_PIXEL_FORMAT_RGB_565)
    {
        last_screen = (GLubyte*)malloc(4 * buffer->stride * buffer->height);
        memcpy(last_screen, buffer_vaddr, 4 * buffer->stride * buffer->height);
    }
    else
    {
        cerr << "unhandled pixel format: " << buffer->format << endl;
        err = 1;
        goto quit;
    }

    last_pixel_format = buffer->format;

quit:
    return err;
}

int renderer_t::dummy_draw()
{
#if DEBUG
    cout << "dummy draw" << endl;
#endif
    if(last_screen != nullptr)
    {
        return draw_raw(last_screen, buffer->stride, buffer->height, last_pixel_format);
    }

    return -1;
}

void renderer_t::lost_focus()
{
    if(save_screen() == 0)
    {
        dummy_draw();
    }
    else
    {
        cerr << "failed to save screen" << endl;
    }
    eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    have_focus = false;
}

void renderer_t::gained_focus()
{
    frames_since_focus_gained = 0;
    eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx);
    have_focus = true;
}

bool renderer_t::is_active()
{
    return have_focus;
}

int renderer_t::render_buffer(ANativeWindowBuffer *the_buffer, buffer_info_t &info)
{
    if(frames_since_focus_gained > 30)
    {
        pfn_eglHybrisWaylandPostBuffer((EGLNativeWindowType)w_egl_window, the_buffer);
        buffer = the_buffer;
        if(eglGetError() != EGL_SUCCESS)
        {
            return 1;
        }
    }
    else frames_since_focus_gained++;

    return 0;
}

int renderer_t::get_height()
{
    return win_height;
}

int renderer_t::get_width()
{
    return win_width;
}

