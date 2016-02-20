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

#include <system/window.h>
#include <SDL_syswm.h>
#include <wayland-egl.h>

using namespace std;

int renderer_t::init()
{
    int err = 0;
    SDL_SysWMinfo info;
    GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };
    EGLConfig egl_cfg;
    EGLint contextParams[] = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE};
    EGLint numConfigs;

#if DEBUG
    cout << "initializing SDL" << endl;
#endif
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        cerr << "SDL_Init failed" << endl;
        err = 1;
        goto quit;
    }

#if DEBUG
    cout << "creating SDL window" << endl;
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    window = SDL_CreateWindow("sfdroid", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
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
    cout << "getting eglCreateImageKHR" << endl;
#endif
    pfn_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if(pfn_eglCreateImageKHR == NULL)
    {
        cerr << "eglCreateImageKHR not found" << endl;
        err = 11;
        goto quit;
    }

#if DEBUG
    cout << "getting glEGLImageTargetTexture2DOES" << endl;
#endif
    pfn_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if(pfn_glEGLImageTargetTexture2DOES == NULL)
    {
        cerr << "glEGLImageTargetTexture2DOES not found" << endl;
        err = 12;
        goto quit;
    }

#if DEBUG
    cout << "getting eglDestroyImageKHR" << endl;
#endif
    pfn_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if(pfn_eglDestroyImageKHR == NULL)
    {
        cerr << "eglDestroyImageKHR not found" << endl;
        err = 13;
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

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &dummy_tex);
    glBindTexture(GL_TEXTURE_2D, dummy_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#if DEBUG
    cout << "loading gralloc module" << endl;
#endif
    if(hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&gralloc_module) != 0)
    {
        cerr << "failed to open " << GRALLOC_HARDWARE_MODULE_ID << " module" << endl;
        err = 4;
        goto quit;
    }

quit:
    return err;
}

void renderer_t::deinit()
{
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &dummy_tex);
    eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_dpy, egl_surf);
    wl_egl_window_destroy(w_egl_window);
    eglDestroyContext(egl_dpy, egl_ctx);
    eglTerminate(egl_dpy);
    if(last_screen) free(last_screen);
    last_screen = nullptr;
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void dummy_f(android_native_base_t *base)
{
}

int renderer_t::render_buffer(native_handle_t *the_buffer, buffer_info_t &info)
{
    EGLImageKHR egl_img;
    int err = 0;
    int gerr = 0;
    int registered = 0;

    float xf = 1.f;
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

    glBindTexture(GL_TEXTURE_2D, tex);

    glVertexPointer(2, GL_FLOAT, 0, &vtxcoords);
    glTexCoordPointer(2, GL_FLOAT, 0, &texcoords);
 
    gerr = gralloc_module->registerBuffer(gralloc_module, the_buffer);
    if(gerr)
    {
        cerr << "registerBuffer failed: " << strerror(-gerr) << endl;
        err = 1;
        goto quit;
    }
    registered = 1;

    buffer = new ANativeWindowBuffer();
    buffer->width = info.width;
    buffer->height = info.height;
    buffer->stride = info.stride;
    buffer->format = info.pixel_format;
    buffer->handle = the_buffer;
    buffer->common.incRef = dummy_f;
    buffer->common.decRef = dummy_f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, win_width, win_height, 0, 0, 1);

#if DEBUG
    cout << "creating egl image from buffer" << endl;
#endif
    egl_img = pfn_eglCreateImageKHR(egl_dpy, egl_ctx, EGL_NATIVE_BUFFER_ANDROID, buffer, NULL);
    if(egl_img == EGL_NO_IMAGE_KHR)
    {
        cerr << "failed to create egl image " << hex << eglGetError() << endl;
        err = 3;
        goto quit;
    }

#if DEBUG
    cout << "binding image to texture" << endl;
#endif
    pfn_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_img);

#if DEBUG
    cout << "drawing texture" << endl;
#endif
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    eglSwapBuffers(egl_dpy, egl_surf);

    pfn_eglDestroyImageKHR(egl_dpy, egl_img);

    delete buffer;
    buffer = nullptr;

    if(last_screen) free(last_screen);
    last_screen = nullptr;

quit:
    if(buffer) delete buffer;
    buffer = nullptr;
    if(registered)
    {
        gralloc_module->unregisterBuffer(gralloc_module, the_buffer);
        registered = 0;
    }

    return err;
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

    if(pixel_format == HAL_PIXEL_FORMAT_RGBA_8888 || pixel_format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    else if(pixel_format == HAL_PIXEL_FORMAT_RGB_565)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
    }
    else
    {
        cerr << "unhandled pixel format: " << pixel_format << endl;
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

int renderer_t::save_screen(int pixel_format)
{
    int err = 0;

    if(last_screen) free(last_screen);
    last_screen = nullptr;

    if(pixel_format == HAL_PIXEL_FORMAT_RGBA_8888 || pixel_format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        last_screen = (GLubyte*)malloc(4 * win_width * win_height);
        glReadPixels(0, 0, win_width, win_height, GL_RGBA, GL_UNSIGNED_BYTE, last_screen);
    }
    else if(pixel_format == HAL_PIXEL_FORMAT_RGB_565)
    {
        last_screen = (GLubyte*)malloc(4 * win_width * win_height);
        glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, last_screen);
    }
    else
    {
        cerr << "unhandled pixel format: " << pixel_format << endl;
        err = 1;
        goto quit;
    }

    last_pixel_format = pixel_format;

quit:
    return err;
}

int renderer_t::dummy_draw()
{
#if DEBUG
    cout << "dummy draw" << endl;
#endif
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, win_width, 0, win_height, 0, 1);
    if(last_screen != nullptr)
    {
        return draw_raw(last_screen, win_width, win_height, last_pixel_format);
    }

    return -1;
}

int renderer_t::get_height()
{
    return win_height;
}

int renderer_t::get_width()
{
    return win_width;
}

