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

#include "sensorconnection.h"

#include <iostream>

#include <SDL.h>
#include <QCoreApplication>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace std;

int sensorconnection_t::init()
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
    strncpy(addr.sun_path, SENSORS_HANDLE_FILE, sizeof(addr.sun_path)-1);

    unlink(SENSORS_HANDLE_FILE);

    if(bind(fd_pass_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        cerr << "failed to bind socket" << SENSORS_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "listening on " << SENSORS_HANDLE_FILE << endl;
#endif
    if(listen(fd_pass_socket, 5) < 0)
    {
        cerr << "failed to listen on socket " << SENSORS_HANDLE_FILE << ": " << strerror(errno) << endl;
        err = 3;
        goto quit;
    }

    chmod(SENSORS_HANDLE_FILE, 0770);

quit:
    return err;
}

int sensorconnection_t::wait_for_client()
{
    int err = 0;

#if DEBUG
    cout << "waiting for client (sensors module)" << endl;
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

void sensorconnection_t::update_timeout()
{
    struct timeval timeout;
    memset(&timeout, 0, sizeof(timeout));

    if(have_focus)
    {
        timeout.tv_usec = SENSOR_SOCKET_TIMEOUT_US;
    }
    else
    {
        timeout.tv_sec = SENSOR_SOCKET_FOCUS_LOST_TIMEOUT_S;
    }

    if(setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        cerr << "failed to set timeout on sensor socket: " << strerror(errno) << endl;
    }
}

void sensorconnection_t::thread_loop()
{
    int argc;
    char *argv[0];
    QCoreApplication app(argc, argv);
    int err = 0;

    remoteSensorManager = &SensorManagerInterface::instance();

    if(!remoteSensorManager->isValid())
    {
        cerr << "remoteSensorManager is not valid" << endl;
        err = 1;
        goto quit;
    }

    remoteSensorManager->loadPlugin("accelerometersensor");
    remoteSensorManager->registerSensorInterface<AccelerometerSensorChannelInterface>("accelerometersensor");

    accel = AccelerometerSensorChannelInterface::interface("accelerometersensor");

    if(!accel || !accel->isValid())
    {
        cerr << "could not get accelerometersensor" << endl;
        err = 2;
        goto quit;
    }

    accel->setInterval(100);

    accel->start();
    running = true;

    while(running)
    {
        if(have_client()) update_timeout();

        if(!have_client())
        {
            wait_for_client();
        }

        if(have_client())
        {
            int type, timedout;
            if(wait_for_request(type, timedout) == 0)
            {
                if(!timedout)
                {
                    if(type == ACCELEROMETER)
                    {
                        send_accelerometer_data();
                    }
                }
            }
        }

        std::this_thread::yield();
    }

    if(accel) accel->stop();

    close(fd_client);
    fd_client = -1;

quit:
    if(err != 0) cerr << "not starting the sensors thread" << endl;
    return;
}

int sensorconnection_t::wait_for_request(int &type, int &timedout)
{
    int err = 0;
    int64_t delay;
    int enable;
    char syncbuf[1];

    timedout = 0;

#if DEBUG
    cout << "waiting for sensor request" << endl;
#endif
    char buffer[256];
    int len;

    len = recv(fd_client, syncbuf, 1, MSG_WAITALL);
    if(len < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN || errno == EINTR)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "sensors: lost client" << endl;
        err = 1;
        goto quit;
    }

    len = recv(fd_client, buffer, syncbuf[0], MSG_WAITALL);
    if(len < 0)
    {
        if(errno == ETIMEDOUT || errno == EAGAIN)
        {
            timedout = 1;
            err = 0;
            goto quit;
        }

        cerr << "sensors: lost client" << endl;
        err = 1;
        goto quit;
    }

    buffer[len] = 0;

    if(strcmp(buffer, "get:accelerometer") == 0)
    {
#if DEBUG
        cout << "received the get:accelerometer command" << endl;
#endif
        type = ACCELEROMETER;
    }
    else if(sscanf(buffer, "setDelay:acceleration:%lld", &delay) == 1)
    {
#if DEBUG
        cout << "setting accelerometer interval " << delay << endl;
#endif
        accel->setInterval(delay / 1000000);
    }
    else if(sscanf(buffer, "set:acceleration:%d", &enable) == 1)
    {
#if DEBUG
        cout << "setting accelerometer enabled: " << enable << endl;
#endif
        if(enable) accel->start();
        else accel->stop();
    }
    else
    {
        cerr << "unknown request: " << buffer << endl;
        type = -1;
        err = 1;
        goto quit;
    }

quit:
    if(err != 0)
    {
        close(fd_client);
        fd_client = -1;
    }
    return err;
}

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298

int sensorconnection_t::send_accelerometer_data()
{
    int err = 0;
    int r = 0;
    char buffer[512];
    char syncbuf[1];
    XYZ a = accel->get();
    int64_t timestamp;
    struct timespec  ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    timestamp = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;

    double x = (double)a.x() / GRAVITY_RECIPROCAL_THOUSANDS;
    double y = (double)a.y() / GRAVITY_RECIPROCAL_THOUSANDS;
    double z = (double)a.z() / GRAVITY_RECIPROCAL_THOUSANDS;

#if DEBUG
    cout << "accelerometer info: x " << x << " y " << y << " z " << z << endl;
#endif
    sprintf(buffer, "acceleration:%g:%g:%g:%lld", x, y, z, timestamp);

    syncbuf[0] = strlen(buffer) + 1;
    r = send(fd_client, syncbuf, 1, 0);
    if(r < 0)
    {
        cerr << "failed to send sync byte" << endl;
        err = 1;
        goto quit;
    }

    r = send(fd_client, buffer, syncbuf[0], 0);
    if(r < 0)
    {
        cerr << "failed to send accelerometer data" << endl;
        err = 1;
        goto quit;
    }

quit:
    if(err != 0)
    {
        close(fd_client);
        fd_client = -1;
    }
    return err;
}

bool sensorconnection_t::have_client()
{
    return (fd_client >= 0);
}

void sensorconnection_t::deinit()
{
    if(fd_pass_socket >= 0) close(fd_pass_socket);
    if(fd_client >= 0) close(fd_client);
    unlink(SENSORS_HANDLE_FILE);
}

void sensorconnection_t::start_thread()
{
    my_thread = std::thread(&sensorconnection_t::thread_loop, this);
}

void sensorconnection_t::stop_thread()
{
    running = false;
    if(fd_client >= 0) shutdown(fd_client, SHUT_RDWR);
    if(fd_pass_socket >= 0) shutdown(fd_pass_socket, SHUT_RDWR);
    my_thread.join();
}

void sensorconnection_t::lost_focus()
{
    have_focus = false;
}

void sensorconnection_t::gained_focus()
{
    have_focus = true;
}

