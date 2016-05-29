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

#include <iostream>

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>

#include "sfdroid_defs.h"
#include "uinput.h"
#include "utility.h"

using namespace std;

int uinput_t::init(int win_width, int win_height)
{
    int err = 0;
#if DEBUG
    cout << "setting up uinput" << endl;
#endif
    // try different uinput device nodes
    int errnos[3];
    fd_uinput = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    errnos[0] = errno;
    if(fd_uinput < 0)
    {
        fd_uinput = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
        errnos[1] = errno;
    }
    if(fd_uinput < 0)
    {
        fd_uinput = open("/dev/misc/uinput", O_WRONLY | O_NONBLOCK);
        errnos[2] = errno;
    }
    if(fd_uinput < 0)
    {
        cerr << "failed to open uinput device:" << endl;
        cerr << "/dev/uinput: " << strerror(errnos[0]) << endl;
        cerr << "/dev/input/uinput: " << strerror(errnos[1]) << endl;
        cerr << "/dev/misc/uinput: " << strerror(errnos[2]) << endl;
        for(unsigned int i=0;i<sizeof(errnos)/sizeof(int);i++)
        {
            if(errnos[i] == EACCES)
            {
                cerr << "wrong permissions, allow nemo to use uinput." << endl;
                break;
            }
        }
        err = 17;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_EVBIT, EV_SYN) < 0)
    {
        cerr << "UI_SET_EVBIT EV_SYN ioctl failed" << endl;
        err = 20;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_EVBIT, EV_ABS) < 0)
    {
        cerr << "UI_SET_EVBIT EV_ABS ioctl failed" << endl;
        err = 21;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_ABSBIT, ABS_MT_TRACKING_ID) < 0)
    {
        cerr << "UI_SET_ABSBIT ABS_MT_TRACKING_ID ioctl failed" << endl;
        err = 22;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_ABSBIT, ABS_MT_BLOB_ID) < 0)
    {
        cerr << "UI_SET_ABSBIT ABS_MT_SLOT ioctl failed" << endl;
        err = 23;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_ABSBIT, ABS_MT_POSITION_X) < 0)
    {
        cerr << "UI_SET_ABSBIT ABS_MT_POSITION_X ioctl failed" << endl;
        err = 24;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_SET_ABSBIT, ABS_MT_POSITION_Y) < 0)
    {
        cerr << "UI_SET_ABSBIT ABS_MT_POSITION_Y ioctl failed" << endl;
        err = 25;
        goto quit;
    }

/*
    if(ioctl(fd_uinput, UI_SET_ABSBIT, ABS_MT_PRESSURE) < 0)
    {
        cerr << "UI_SET_ABSBIT ABS_MT_PRESSURE ioctl failed" << endl;
        err = 28;
        goto quit;
    }
*/
/*
    if(ioctl(fd_uinput, UI_SET_PROPBIT, INPUT_PROP_DIRECT) < 0)
    {
        cerr << "UI_SET_PROPBIT INPUT_PROP_DIRECT ioctl failed" << endl;
        err = 26;
        goto quit;
    }
*/
    memset(&uidev, 0, sizeof(uidev));

    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "sfdroid-input");

    uidev.absmin[ABS_MT_POSITION_X] = 0;
    uidev.absmax[ABS_MT_POSITION_X] = win_width;
    uidev.absmin[ABS_MT_POSITION_Y] = 0;
    uidev.absmax[ABS_MT_POSITION_Y] = win_height;
/*
    uidev.absmin[ABS_MT_PRESSURE] = 0;
    uidev.absmax[ABS_MT_PRESSURE] = MAX_PRESSURE;
*/
    // hmm
    uidev.absmax[ABS_MT_BLOB_ID] = 255;
    uidev.absmax[ABS_MT_TRACKING_ID] = 255;

    uidev.id.bustype = BUS_VIRTUAL;
    // hmm
    uidev.id.vendor = 0x1;
    uidev.id.product = 0x1;
    uidev.id.version = 1;

    if(write(fd_uinput, &uidev, sizeof(uidev)) < 0)
    {
        cerr << "failed to write sfdroid-input structure to uinput: " << strerror(errno) << endl;
        err = 18;
        goto quit;
    }

    if(ioctl(fd_uinput, UI_DEV_CREATE) < 0)
    {
        cerr << "failed to create sfdroid-input uinput device: " << strerror(errno) << endl;
        err = 19;
        goto quit;
    }
    uinput_dev_created = 1;

quit:
    return err;
}

int uinput_t::send_event(int type, int code, int value)
{
    struct input_event ev;

    gettimeofday(&ev.time, NULL);

    ev.type = type;
    ev.code = code;
    ev.value = value;

    if(write(fd_uinput, &ev, sizeof(ev)) < 0)
    {
        return 0;
    }

    return 1;
}

void uinput_t::deinit()
{
    if(uinput_dev_created) ioctl(fd_uinput, UI_DEV_DESTROY);
    if(fd_uinput >= 0) close(fd_uinput);
}

