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

#include "sfdroid_defs.h"

#include <sys/socket.h>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <errno.h>

using namespace std;

int recv_native_handle(int fd, native_handle_t **handle, struct buffer_info_t *info)
{
    struct msghdr socket_message;
    struct iovec io_vector[1];
    struct cmsghdr *control_message = NULL;
    unsigned int buffer_size = sizeof(struct buffer_info_t) + sizeof(native_handle_t) + sizeof(int)*(MAX_NUM_FDS + MAX_NUM_INTS);
    unsigned int handle_size = sizeof(native_handle_t) + sizeof(int)*(MAX_NUM_FDS + MAX_NUM_INTS);
    char message_buffer[buffer_size];
    char ancillary_buffer[CMSG_SPACE(sizeof(int) * MAX_NUM_FDS)];

    *handle = (native_handle_t*)malloc(handle_size);
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

    if(recvmsg(fd, &socket_message, MSG_CMSG_CLOEXEC) < 0)
    {
        if(errno != ETIMEDOUT && errno != EAGAIN) cerr << "recvmsg failed: " << strerror(errno) << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    memcpy(info, message_buffer, sizeof(struct buffer_info_t));
    memcpy(*handle, message_buffer + sizeof(struct buffer_info_t), sizeof(native_handle_t));

    if((*handle)->numFds > MAX_NUM_FDS)
    {
        cerr << "too less space reserved for fds: " << (*handle)->numFds << " > " << MAX_NUM_FDS << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    if((*handle)->numInts > MAX_NUM_INTS)
    {
        cerr << "too less space reserved for ints: " << (*handle)->numInts << " > " << MAX_NUM_INTS << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    *handle = (native_handle_t*)realloc(*handle, sizeof(native_handle_t) + sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    memcpy((char*)*handle + sizeof(native_handle_t), message_buffer + sizeof(struct buffer_info_t) + sizeof(native_handle_t), sizeof(int)*((*handle)->numFds + (*handle)->numInts));

    if(socket_message.msg_flags & MSG_CTRUNC)
    {
        cerr << "not enough space in the ancillary buffer" << endl;
        free(*handle);
        *handle = NULL;
        return -1;
    }

    for(int i=0;i<(*handle)->numFds;i++)
    {
        ((native_handle_t*)(*handle))->data[i] = ((int*)CMSG_DATA(control_message))[i];
    }

    return 0;
}

int send_status(int fd, int failed)
{
    char message_buffer[3];
    if(failed) memcpy(message_buffer, "FA", sizeof(message_buffer));
    else memcpy(message_buffer, "OK", sizeof(message_buffer));

    return send(fd, message_buffer, sizeof(message_buffer), MSG_NOSIGNAL);
}

void free_handle(native_handle_t *handle)
{
    for(int i=0;i<handle->numFds;i++)
    {
        close(handle->data[i]);
    }
    free((void*)handle);
}

