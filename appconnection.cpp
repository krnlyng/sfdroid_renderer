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

#include "appconnection.h"

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace std;

int appconnection_t::init(uint32_t the_sdl_event)
{
    int err = 0;
    struct sockaddr_un addr;

    sdl_event = the_sdl_event;

    fd_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd_socket < 0)
    {
        cerr << "failed to create socket: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, APP_HELPERS_HANDLE_FILE, sizeof(addr.sun_path)-1);

    unlink(APP_HELPERS_HANDLE_FILE);

    if(bind(fd_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        cerr << "failed to bind socket" << APP_HELPERS_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "listening on " << APP_HELPERS_HANDLE_FILE << endl;
#endif
    if(listen(fd_socket, 5) < 0)
    {
        cerr << "failed to listen on socket " << APP_HELPERS_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 3;
        goto quit;
    }

    chmod(APP_HELPERS_HANDLE_FILE, 0770);

quit:
    return err;
}

int appconnection_t::wait_for_client()
{
    int err = 0;

#if DEBUG
    cout << "waiting for client (appconnection module)" << endl;
#endif
    if((fd_client = accept(fd_socket, NULL, NULL)) < 0)
    {
        cerr << "failed to accept: " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    update_timeout();

quit:
    return err;
}

void appconnection_t::update_timeout()
{
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    timeout.tv_sec = APP_SOCKET_TIMEOUT_S;

    if(setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "failed to set timeout on appconnection socket: " << strerror(errno) << endl;
    }
}

std::string appconnection_t::get_new_window()
{
    return new_window;
}

void appconnection_t::thread_loop()
{
    running = true;

    while(running)
    {
        if(!have_client())
        {
            wait_for_client();
        }

        if(have_client())
        {
            int type, timedout;
            string app;
            if(wait_for_request(type, timedout, app) == 0)
            {
                if(!timedout)
                {
                    SDL_Event event;
                    SDL_memset(&event, 0, sizeof(event));
                    event.type = sdl_event;

                    if(app.find("close:") != std::string::npos)
                    {
                        app = app.substr(app.find("close:") + 6);

                        // todo wait for main to have processed
                        new_window = app;
                        event.user.code = CLOSE_APP;
                    }
                    else
                    {
                        new_window = app;
                        event.user.code = START_APP;
                    }

                    // tell main to show/close
                    SDL_PushEvent(&event);
                }
            }
        }

        std::this_thread::yield();
    }

    close(fd_client);
    fd_client = -1;
}

int appconnection_t::wait_for_request(int &type, int &timedout, string &app)
{
    int err = 0;
    char syncbuf[2];

    timedout = 0;

#if DEBUG
    cout << "waiting for appconnection request" << endl;
#endif
    char buffer[256];
    int16_t len;

    len = recv(fd_client, syncbuf, 2, MSG_WAITALL);
    if(len < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "appconnection: lost client " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    memcpy(&len, syncbuf, 2);
    len = recv(fd_client, buffer, len, MSG_WAITALL);
    if(len < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "appconnection: lost client " << strerror(errno) << endl;
        err = 1;
        goto quit;
    }

    buffer[len] = 0;
    app = buffer;

quit:
    if(err != 0)
    {
        close(fd_client);
        fd_client = -1;
    }
    return err;
}

bool appconnection_t::have_client()
{
    return (fd_client >= 0);
}

void appconnection_t::deinit()
{
    if(fd_socket >= 0) close(fd_socket);
    if(fd_client >= 0) close(fd_client);
    unlink(APP_HELPERS_HANDLE_FILE);
}

void appconnection_t::start_thread()
{
    my_thread = std::thread(&appconnection_t::thread_loop, this);
}

void appconnection_t::stop_thread()
{
    running = false;
    if(fd_client >= 0) shutdown(fd_client, SHUT_RDWR);
    if(fd_socket >= 0) shutdown(fd_socket, SHUT_RDWR);
    my_thread.join();
}

