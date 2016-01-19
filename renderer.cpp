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
#include <GLES/glext.h>

using namespace std;

int renderer_t::init()
{
    int err = 0;

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

#if DEBUG
    cout << "creating SDL GL context" << endl;
#endif
    glcontext = SDL_GL_CreateContext(window);
    if(glcontext == NULL)
    {
        cerr << "failed to create SDL GL Context" << endl;
        err = 3;
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
    if(glcontext) SDL_GL_DeleteContext(glcontext);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}

int renderer_t::render_buffer(native_handle_t *the_buffer, buffer_info_t &info)
{
    void *buffer_vaddr = NULL;
    int registered = 0;
    int err = 0;
    int gerr = 0;
    GLuint gl_err = 0;

    float xf = (float)win_width / (float)info.stride;
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

    gerr = gralloc_module->registerBuffer(gralloc_module, the_buffer);
    if(gerr)
    {
        cerr << "registerBuffer failed: " << strerror(-gerr) << endl;
        err = 1;
        goto quit;
    }
    registered = 1;

    gerr = gralloc_module->lock(gralloc_module, the_buffer,
        GRALLOC_USAGE_SW_READ_RARELY,
        0, 0, info.width, info.height,
        &buffer_vaddr);
    if(gerr)
    {
        cerr << "failed to lock gralloc buffer" << endl;
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "drawing buffer" << endl;
#endif
    if(info.pixel_format == HAL_PIXEL_FORMAT_RGBA_8888 || info.pixel_format == HAL_PIXEL_FORMAT_RGBX_8888)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.stride, info.height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, buffer_vaddr);

    }
    else if(info.pixel_format == HAL_PIXEL_FORMAT_RGB_565)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, info.stride, info.height, 0,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer_vaddr);
    }
    else
    {
        cerr << "unhandled pixel format: " << info.pixel_format << endl;
        err = 3;
        goto quit;
    }
    gl_err = glGetError();
    if(gl_err != GL_NO_ERROR) cout << "glGetError(): " << gl_err << endl;

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(window);

    gralloc_module->unlock(gralloc_module, the_buffer);

quit:
    if(registered)
    {
        gralloc_module->unregisterBuffer(gralloc_module, the_buffer);
        registered = 0;
    }

    return err;
}

