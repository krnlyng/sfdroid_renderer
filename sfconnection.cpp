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

#define _GLIBCXX_USE_SCHED_YIELD

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <SDL.h>

#include "sfconnection.h"
#include "utility.h"

using namespace std;

gralloc_module_t *gralloc_module(NULL);

int sfconnection_t::init(uint32_t the_sdl_event)
{
    int err = 0;
    struct sockaddr_un addr;

    fd_pass_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd_pass_socket < 0)
    {
        cerr << "failed to create socket: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SHAREBUFFER_HANDLE_FILE, sizeof(addr.sun_path)-1);

    unlink(SHAREBUFFER_HANDLE_FILE);

    if(::bind(fd_pass_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        cerr << "failed to bind socket" << SHAREBUFFER_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "listening on " << SHAREBUFFER_HANDLE_FILE << endl;
#endif
    if(listen(fd_pass_socket, 5) < 0)
    {
        cerr << "failed to listen on socket " << SHAREBUFFER_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 3;
        goto quit;
    }

#if DEBUG
    cout << "loading gralloc module" << endl;
#endif
    if(hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&gralloc_module) != 0)
    {
        cerr << "failed to open " << GRALLOC_HARDWARE_MODULE_ID << " module" << endl;
        err = 4;
        goto quit;
    }

    chmod(SHAREBUFFER_HANDLE_FILE, 0770);

    sdl_event = the_sdl_event;

quit:
    return err;
}

void dummy_f(android_native_base_t *base)
{
}

int sfconnection_t::wait_for_buffer(int &timedout)
{
    int err = 0;
    int r;
    int gerr;
    char buf[1];
    ANativeWindowBuffer *buffer = NULL;
    native_handle_t *handle = NULL;
    int registered = 0;
    timedout = 0;

#if DEBUG
    cout << "waiting for notification" << endl;
#endif
    r = recv(fd_client, buf, 1, 0);
    if(r < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "lost client " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    if(buf[0] != 0xFF)
    {
#if DEBUG
        cout << "received post notification" << endl;
#endif
        unsigned int index = buf[0];
        if(index < 0 || index > buffers.size())
        {
            cerr << "invalid index: " << index << endl;
            err = 1;
            goto quit;
        }

        current_buffer = buffers[index];
        current_info = buffer_infos[index];

        err = 0;
        goto quit;
    }

    buffer = new ANativeWindowBuffer();

#if DEBUG
    cout << "waiting for handle" << endl;
#endif
    r = recv_native_handle(fd_client, &handle, &current_info);
    if(r < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "lost client" << endl;
        err = 1;
        goto quit;
    }
 
    gerr = gralloc_module->registerBuffer(gralloc_module, handle);
    if(gerr)
    {
        cerr << "registerBuffer failed: " << strerror(-gerr) << endl;
        err = 1;
        goto quit;
    }

    buffer->width = current_info.width;
    buffer->height = current_info.height;
    buffer->stride = current_info.stride;
    buffer->format = current_info.pixel_format;
    buffer->handle = handle;
    buffer->common.incRef = dummy_f;
    buffer->common.decRef = dummy_f;

    current_buffer = buffer;

    buffers.push_back(current_buffer);
    buffer_infos.push_back(current_info);

#if DEBUG
    cout << "buffer info:" << endl;
    cout << "width: " << current_info.width << " height: " << current_info.height << " stride: " << current_info.stride << " pixel_format: " << current_info.pixel_format << endl;
#endif
    quit:
    if(err != 0)
    {
        if(registered)
        {
            gralloc_module->unregisterBuffer(gralloc_module, handle);
        }
        close(fd_client);
        fd_client = -1;
        remove_buffers();
        if(buffer) delete buffer;
        if(handle)
        {
            free_handle(handle);
        }
    }
    return err;
}

void sfconnection_t::remove_buffers()
{
    for(std::vector<ANativeWindowBuffer*>::size_type i = 0;i < buffers.size();i++)
    {
        ANativeWindowBuffer *buffer = buffers[i];
        const native_handle_t *handle = buffer->handle;

        gralloc_module->unregisterBuffer(gralloc_module, handle);

        for(int i=0;i<handle->numFds;i++)
        {
            close(handle->data[i]);
        }
        free((void*)handle);

        delete buffer;
    }

    buffers.resize(0);
    buffer_infos.resize(0);
}

void sfconnection_t::send_status_and_cleanup()
{
    if(fd_client >= 0)
    {
#if DEBUG
        cout << "sending status" << endl;
#endif
        if(send_status(fd_client, current_status) < 0)
        {
            cerr << "lost client" << endl;
            close(fd_client);
            fd_client = -1;
            remove_buffers();
        }
    }
}

buffer_info_t *sfconnection_t::get_current_info()
{
    return &current_info;
}

ANativeWindowBuffer *sfconnection_t::get_current_buffer()
{
    return current_buffer;
}

int sfconnection_t::wait_for_client()
{
    int err = 0;

#if DEBUG
    cout << "waiting for client (sharebuffer module)" << endl;
#endif
    if((fd_client = accept(fd_pass_socket, NULL, NULL)) < 0)
    {
        cerr << "failed to accept: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    update_timeout();

quit:
    return err;
}

void sfconnection_t::update_timeout()
{
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    if(have_focus)
    {
        timeout.tv_usec = SHAREBUFFER_SOCKET_TIMEOUT_US;
    }
    else
    {
        timeout.tv_sec = SHAREBUFFER_SOCKET_FOCUS_LOST_TIMEOUT_S;
    }

    if(setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "failed to set timeout on sharebuffer socket: " << strerror(errno) << endl;
    }
}

void sfconnection_t::thread_loop()
{
    running = true;

    while(running)
    {
        if(have_client()) update_timeout();

        if(!have_client())
        {
#if DEBUG
            cout << "waking up android" << endl;
#endif
            wakeup_android();
            if(wait_for_client() != 0)
            {
                cerr << "waiting for client failed" << endl;
            }
#if DEBUG
            else
            {
                cout << "new client" << endl;
            }
#endif
        }

        if(have_client())
        {
            int timedout = 0;
            if(wait_for_buffer(timedout) == 0)
            {
                buffer_done = false;

                if(!timedout)
                {
                    // tell the renderer to draw the buffer
                    SDL_Event event;
                    SDL_memset(&event, 0, sizeof(event));
                    event.type = sdl_event;
                    event.user.code = BUFFER;
                    SDL_PushEvent(&event);

                    while(!buffer_done)
                    {
                        usleep(1666);
                        std::this_thread::yield();
                        if(!running) break;
                    }

                    // let sharebuffer know were done
                    send_status_and_cleanup();

                    timeout_count = 0;
                }
                else
                {
                    if(((timeout_count + 1) * SHAREBUFFER_SOCKET_TIMEOUT_US) / 1000 >= DUMMY_RENDER_TIMEOUT_MS)
                    {
                        SDL_Event event;
                        SDL_memset(&event, 0, sizeof(event));
                        event.type = sdl_event;
                        event.user.code = NO_BUFFER;
                        SDL_PushEvent(&event);

                        while(!buffer_done)
                        {
                            usleep(1666);
                            std::this_thread::yield();
                            if(!running) break;
                        }

                        timeout_count = 0;
                    }
                    else timeout_count++;

                    if(have_focus)
                    {
#if DEBUG
                        cout << "wakeing up android" << endl;
#endif
                        wakeup_android();
                    }
                }
            }
        }

        std::this_thread::yield();
    }
}

void sfconnection_t::notify_buffer_done(int failed)
{
    current_status = failed;
    buffer_done = true;
}

void sfconnection_t::start_thread()
{
    my_thread = std::thread(&sfconnection_t::thread_loop, this);
}

void sfconnection_t::stop_thread()
{
    running = false;
    if(fd_client >= 0) shutdown(fd_client, SHUT_RDWR);
    if(fd_pass_socket >= 0) shutdown(fd_pass_socket, SHUT_RDWR);
    my_thread.join();
}

bool sfconnection_t::have_client()
{
    return (fd_client >= 0);
}

void sfconnection_t::lost_focus()
{
    have_focus = false;
}

void sfconnection_t::gained_focus()
{
    have_focus = true;
}

void sfconnection_t::deinit()
{
    remove_buffers();
    if(fd_pass_socket >= 0) close(fd_pass_socket);
    if(fd_client >= 0) close(fd_client);
    unlink(SHAREBUFFER_HANDLE_FILE);
}

