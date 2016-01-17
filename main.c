/*
 *  sfdroid
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

#include <SDL.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <linux/fb.h>

#include <hardware/gralloc.h>
#include <hardware/hardware.h>

#define SFDROID_ROOT "/tmp/sfdroid/"
#define SHM_BUFFER_HANDLE_FILE (SFDROID_ROOT "/gralloc_buffer_handle")
#define FOCUS_FILE (SFDROID_ROOT "/have_focus")

// hmmm
#define MAX_NUM_FDS 32
#define MAX_NUM_INTS 32

struct buffer_info_t
{
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    int32_t pixel_format;
};

int recv_native_handle(int fd, native_handle_t **handle, struct buffer_info_t *info)
{
    struct msghdr socket_message;
    struct iovec io_vector[1];
    struct cmsghdr *control_message = NULL;
    unsigned int buffer_size = sizeof(struct buffer_info_t) + sizeof(native_handle_t) + sizeof(int)*(MAX_NUM_FDS + MAX_NUM_INTS);
    unsigned int handle_size = sizeof(native_handle_t) + sizeof(int)*(MAX_NUM_FDS + MAX_NUM_INTS);
    char message_buffer[buffer_size];
    char ancillary_buffer[CMSG_SPACE(sizeof(int) * MAX_NUM_FDS)];

    *handle = malloc(handle_size);
    if(!(*handle)) return -1;

    memset(&socket_message, 0, sizeof(struct msghdr));
    memset(ancillary_buffer, 0, CMSG_SPACE(sizeof(int) * MAX_NUM_FDS));

    io_vector[0].iov_base = message_buffer;
    io_vector[0].iov_len = buffer_size;
    socket_message.msg_iov = io_vector;
    socket_message.msg_iovlen = 1;

    socket_message.msg_control = ancillary_buffer;
    socket_message.msg_controllen = CMSG_SPACE(sizeof(int) * MAX_NUM_FDS);

    control_message = CMSG_FIRSTHDR(&socket_message);
    control_message->cmsg_len = socket_message.msg_controllen;
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;

    if(recvmsg(fd, &socket_message, MSG_CMSG_CLOEXEC | MSG_WAITALL) < 0)
    {
        fprintf(stderr, "recvmsg failed: %s\n", strerror(errno));
        free(*handle);
        *handle = NULL;
        return -1;
    }

    memcpy(info, message_buffer, sizeof(struct buffer_info_t));
    memcpy(*handle, message_buffer + sizeof(struct buffer_info_t), sizeof(native_handle_t));

    if((*handle)->numFds > MAX_NUM_FDS)
    {
        fprintf(stderr, "too less space reserved for fds: %d > %d\n", (*handle)->numFds, MAX_NUM_FDS);
        free(*handle);
        *handle = NULL;
        return -1;
    }

    if((*handle)->numInts > MAX_NUM_INTS)
    {
        fprintf(stderr, "too less space reserved for ints: %d > %d\n", (*handle)->numInts, MAX_NUM_INTS);
        free(*handle);
        *handle = NULL;
        return -1;
    }

    *handle = realloc(*handle, sizeof(native_handle_t) + sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    memcpy((char*)*handle + sizeof(native_handle_t), message_buffer + sizeof(struct buffer_info_t) + sizeof(native_handle_t), sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    if(socket_message.msg_flags & MSG_CTRUNC)
    {
        fprintf(stderr, "not enough space in the ancillary buffer\n");
        free(*handle);
        *handle = NULL;
        return -1;
    }

    for(int i=0;i<(*handle)->numFds;i++)
    {
        (*handle)->data[i] = ((int*)CMSG_DATA(control_message))[i];
    }

    return 0;
}

int send_status(int fd, int failed)
{
    char message_buffer[3];
    if(failed) memcpy(message_buffer, "FA", sizeof(message_buffer));
    else memcpy(message_buffer, "OK", sizeof(message_buffer));

    return send(fd, message_buffer, sizeof(message_buffer), MSG_WAITALL | MSG_NOSIGNAL);
}

void touch(char *fname)
{
    fclose(fopen(fname, "w"));
}

int main(int argc, char *argv[])
{
    gralloc_module_t *gralloc_module = NULL;
    native_handle_t *the_buffer = NULL;
    void *buffer_vaddr = NULL;
    struct buffer_info_t buffer_info;

    int err = -1;
    int registered = 0;
    int fd_pass_socket = -1;
    int fd_client = -1; // the client (sharebuffer module)
    struct sockaddr_un addr;

    int gerr = 0;

    int win_width, win_height;

    SDL_Window *window = NULL;
    SDL_GLContext *glcontext = NULL;

#if DEBUG
    printf("initializing SDL\n");
#endif
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init failed\n");
        err = 7;
        goto quit;
    }

#if DEBUG
    printf("creating SDL window\n");
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
    window = SDL_CreateWindow("sfdroid", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if(window == NULL)
    {
        fprintf(stderr, "failed to create SDL window\n");
        err = 15;
        goto quit;
    }

    SDL_GetWindowSize(window, &win_width, &win_height);
#if DEBUG
    printf("window width: %d, height: %d\n", win_width, win_height);
#endif

#if DEBUG
    printf("creating SDL GL context\n");
#endif
    glcontext = SDL_GL_CreateContext(window);
    if(glcontext == NULL)
    {
        fprintf(stderr, "failed to create SDL GL Context\n");
        err = 16;
        goto quit;
    }

#if DEBUG
    printf("setting up sfdroid directory\n");
#endif
    mkdir(SFDROID_ROOT, 0777);
    touch(FOCUS_FILE);

#if DEBUG
    printf("loading gralloc module\n");
#endif
    if(hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&gralloc_module) != 0)
    {
        fprintf(stderr, "failed to open %s module\n", GRALLOC_HARDWARE_MODULE_ID);
        err = 8;
        goto quit;
    }

    fd_pass_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd_pass_socket < 0)
    {
        fprintf(stderr, "failed to create socket: %s\n", strerror(errno));
        err = 9;
        goto quit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SHM_BUFFER_HANDLE_FILE, sizeof(addr.sun_path)-1);

    unlink(SHM_BUFFER_HANDLE_FILE);

    if(bind(fd_pass_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "failed to bind socket %s: %s\n", SHM_BUFFER_HANDLE_FILE, strerror(errno));
        err = 10;
        goto quit;
    }

#if DEBUG
    printf("listening on %s\n", SHM_BUFFER_HANDLE_FILE);
#endif
    if(listen(fd_pass_socket, 5) < 0)
    {
        fprintf(stderr, "failed to listen on socket %s: %s\n", SHM_BUFFER_HANDLE_FILE, strerror(errno));
        err = 11;
        goto quit;
    }

#if DEBUG
    printf("waiting for client (sharebuffer module)\n");
#endif
    if((fd_client = accept(fd_pass_socket, NULL, NULL)) < 0)
    {
        fprintf(stderr, "failed to accept: %s\n", strerror(errno));
        err = 12;
        goto quit;
    }

#if DEBUG
    printf("setting up gl\n");
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

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    float vtxcoords[] = {
        0.f, 0.f,
        (float)win_width, 0.f,
        0.f, (float)win_height,
        (float)win_width, (float)win_height,
    };

    glVertexPointer(2, GL_FLOAT, 0, &vtxcoords);

    SDL_Event e;
    GLint gl_err;
    int failed = 0;
    for(;;)
    {
        while(SDL_PollEvent(&e))
        {
            if(e.type == SDL_QUIT)
            {
                err = 0;
                goto quit;
            }

            if(e.type == SDL_WINDOWEVENT)
            {
                switch(e.window.event)
                {
                    case SDL_WINDOWEVENT_FOCUS_LOST:
#if DEBUG
                        printf("focus lost\n");
#endif
                        unlink(FOCUS_FILE);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
                        printf("focus gained\n");
#endif
                        touch(FOCUS_FILE);
                        break;
                }
            }
        }

        if(fd_client < 0)
        {
#if DEBUG
            printf("waiting for client (sharebuffer module)\n");
#endif
            if((fd_client = accept(fd_pass_socket, NULL, NULL)) < 0)
            {
                fprintf(stderr, "failed to accept: %s\n", strerror(errno));
                err = 13;
                goto quit;
            }
        }

        failed = 0;

#if DEBUG
        printf("waiting for handle\n");
#endif
        if(recv_native_handle(fd_client, &the_buffer, &buffer_info) < 0)
        {
            fprintf(stderr, "lost client\n");
            close(fd_client);
            fd_client = -1;
            failed = 1;
            goto save_end_loop;
        }

#if DEBUG
        printf("buffer info:\n");
        printf("width: %d, height: %d, stride: %d, pixel_format: %d\n", buffer_info.width, buffer_info.height, buffer_info.stride, buffer_info.pixel_format);
#endif

        float xf = (float)win_width / (float)buffer_info.stride;
        float yf = 1.f;
        float texcoords[] = {
            0.f, 0.f,
            xf, 0.f,
            0.f, yf,
            xf, yf,
        };

        glTexCoordPointer(2, GL_FLOAT, 0, &texcoords);

        gerr = gralloc_module->registerBuffer(gralloc_module, the_buffer);
        if(gerr)
        {
            fprintf(stderr, "registerBuffer failed: %s\n", strerror(-gerr));
            failed = 1;
            goto save_end_loop;
        }
        registered = 1;

        gerr = gralloc_module->lock(gralloc_module, the_buffer,
            GRALLOC_USAGE_SW_READ_RARELY,
            0, 0, buffer_info.width, buffer_info.height,
            &buffer_vaddr);
        if(gerr)
        {
            fprintf(stderr, "failed to lock gralloc buffer\n");
            failed = 1;
            goto save_end_loop;
        }

#if DEBUG
        printf("drawing buffer\n");
#endif
        if(buffer_info.pixel_format == HAL_PIXEL_FORMAT_RGBA_8888 || buffer_info.pixel_format == HAL_PIXEL_FORMAT_RGBX_8888)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buffer_info.stride, buffer_info.height, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, buffer_vaddr);

        }
        else if(buffer_info.pixel_format == HAL_PIXEL_FORMAT_RGB_565)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, buffer_info.stride, buffer_info.height, 0,
                    GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer_vaddr);
        }
        else
        {
            fprintf(stderr, "unhandled pixel format: %d\n", buffer_info.pixel_format);
            err = 14;
            goto quit;
        }
        gl_err = glGetError();
        if(gl_err != GL_NO_ERROR) fprintf(stderr, "glGetError(): %d\n", gl_err);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);

        gralloc_module->unlock(gralloc_module, the_buffer);

save_end_loop:
        if(registered)
        {
            gralloc_module->unregisterBuffer(gralloc_module, the_buffer);
            registered = 0;
        }

        if(the_buffer)
        {
            for(int i=0;i<the_buffer->numFds;i++)
            {
                close(the_buffer->data[i]);
            }
            free(the_buffer);
            the_buffer = NULL;
        }

        if(fd_client >= 0)
        {
#if DEBUG
            printf("sending status\n");
#endif
            if(send_status(fd_client, failed) < 0)
            {
                fprintf(stderr, "lost client\n");
                close(fd_client);
                fd_client = -1;
            }
        }
    }

quit:
    glDeleteTextures(1, &tex);
    if(glcontext) SDL_GL_DeleteContext(glcontext);
    if(window) SDL_DestroyWindow(window);
    unlink(SHM_BUFFER_HANDLE_FILE);
    unlink(FOCUS_FILE);
    rmdir(SFDROID_ROOT);
    if(fd_pass_socket >= 0) close(fd_pass_socket);
    if(fd_client >= 0) close(fd_client);
    if(the_buffer)
    {
        for(int i=0;i<the_buffer->numFds;i++)
        {
            close(the_buffer->data[i]);
        }
        free(the_buffer);
    }
    SDL_Quit();
    return err;
}

