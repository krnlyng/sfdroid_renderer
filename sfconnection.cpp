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

#include "sfconnection.h"

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

using namespace std;

int sfconnection_t::init()
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

    if(bind(fd_pass_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
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

    chmod(SHAREBUFFER_HANDLE_FILE, 0770);

quit:
    return err;
}

int sfconnection_t::wait_for_buffer(native_handle_t **handle, buffer_info_t *info, int &timedout)
{
    int err = 0;

    timedout = 0;

#if DEBUG
    cout << "waiting for handle" << endl;
#endif
    int r = recv_native_handle(fd_client, handle, info);
    if(r < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "lost client" << endl;
        close(fd_client);
        fd_client = -1;
        err = 1;
    }

#if DEBUG
    cout << "buffer info:" << endl;
    cout << "width: " << info->width << " height: " << info->height << " stride: " << info->stride << " pixel_format: " << info->pixel_format << endl;
#endif
    quit:
    return err;
}

void sfconnection_t::send_status_and_cleanup(native_handle_t **handle, int failed)
{
    const native_handle_t *the_buffer = *handle;

    for(int i=0;i<the_buffer->numFds;i++)
    {
        close(the_buffer->data[i]);
    }
    free((void*)the_buffer);
    *handle = NULL;

    if(fd_client >= 0)
    {
#if DEBUG
        cout << "sending status" << endl;
#endif
        if(send_status(fd_client, failed) < 0)
        {
            cerr << "lost client" << endl;
            close(fd_client);
            fd_client = -1;
        }
    }
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

    struct timespec timeout;
    memset(&timeout, 0, sizeof(timeout));
    timeout.tv_nsec = SHAREBUFFER_SOCKET_TIMEOUT_NS;

    if(setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "failed to set timeout on sharebuffer socket: " << strerror(errno) << endl;
    }
quit:
    return err;
}

bool sfconnection_t::have_client()
{
    return (fd_client >= 0);
}

void sfconnection_t::deinit()
{
    if(fd_pass_socket >= 0) close(fd_pass_socket);
    if(fd_client >= 0) close(fd_client);
    unlink(SHAREBUFFER_HANDLE_FILE);
}

