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

#include <SDL.h>

#include <sys/stat.h>

#include "renderer.h"
#include "sfconnection.h"
#include "utility.h"
#include "uinput.h"

using namespace std;

int main(int argc, char *argv[])
{
    int err = 0;

    native_handle_t *handle = NULL;
    buffer_info_t info;
    int timedout = 0;
    int have_focus = 0;

    sfconnection_t sfconnection;
    renderer_t renderer;
    uinput_t uinput;
    int first_fingerId = -1; // needed because first slot must be 0

#if DEBUG
    cout << "setting up sfdroid directory" << endl;
#endif
    mkdir(SFDROID_ROOT, 0770);

    if(sfconnection.init() != 0)
    {
        err = 1;
        goto quit;
    }

    if(renderer.init() != 0)
    {
        err = 2;
        goto quit;
    }

#if DEBUG
    cout << "setting up uinput" << endl;
#endif
    if(uinput.init(renderer.get_width(), renderer.get_height()) != 0)
    {
        err = 5;
        goto quit;
    }

#if DEBUG
    cout << "waking up android" << endl;
#endif
    wakeup_android();

    if(sfconnection.wait_for_client() != 0)
    {
        err = 3;
        goto quit;
    }

    SDL_Event e;
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
                        have_focus = 0;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
                        printf("focus gained\n");
#endif
                        wakeup_android();
                        have_focus = 1;
                        break;
                }
            }

            if(have_focus)
            {
#if DEBUG
                int m = 0;
#endif
                switch(e.type)
                {
                    case SDL_FINGERUP:
#if DEBUG
                        printf("SDL_FINGERUP\n");
#endif
                        uinput.send_event(EV_ABS, ABS_MT_SLOT, (e.tfinger.fingerId == first_fingerId) ? 0 : e.tfinger.fingerId);
                        uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
                        uinput.send_event(EV_SYN, SYN_REPORT, 0);
                        if(e.tfinger.fingerId == first_fingerId) first_fingerId = -1;
                        break;
                    case SDL_FINGERMOTION:
#if DEBUG
                        printf("SDL_FINGERMOTION\n");
                        m = 1;
#endif
                    case SDL_FINGERDOWN:
#if DEBUG
                        if(!m) printf("SDL_FINGERDOWN\n");
#endif
                        if(first_fingerId == -1) first_fingerId = e.tfinger.fingerId;

                        uinput.send_event(EV_ABS, ABS_MT_SLOT, (e.tfinger.fingerId == first_fingerId) ? 0 : e.tfinger.fingerId);
                        uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, e.tfinger.fingerId);
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_X, e.tfinger.x);
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_Y, e.tfinger.y);
                        uinput.send_event(EV_ABS, ABS_MT_PRESSURE, e.tfinger.pressure * MAX_PRESSURE);
                        uinput.send_event(EV_SYN, SYN_REPORT, e.tfinger.fingerId);
                        break;
                    default:
                         break;
                 }
             }
        }

        if(!sfconnection.have_client())
        {
#if DEBUG
            cout << "waking up android" << endl;
#endif
            wakeup_android();
            if(sfconnection.wait_for_client() != 0)
            {
                err = 4;
                goto quit;
            }
        }

        if(sfconnection.wait_for_buffer(&handle, &info, timedout) == 0)
        {
            if(!timedout)
            {
                int failed = renderer.render_buffer(handle, info);
                sfconnection.send_status_and_cleanup(&handle, failed);
            }
        }
    }

quit:
    uinput.deinit();
    renderer.deinit();
    sfconnection.deinit();
    rmdir(SFDROID_ROOT);
    return err;
}

