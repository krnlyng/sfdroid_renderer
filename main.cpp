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

    int have_focus = 1;

    sfconnection_t sfconnection;
    renderer_t renderer;
    uinput_t uinput;
    int first_fingerId = -1; // needed because first slot must be 0

    uint32_t our_sdl_event = 0;

#if DEBUG
    cout << "setting up sfdroid directory" << endl;
#endif
    mkdir(SFDROID_ROOT, 0770);

    if(renderer.init() != 0)
    {
        err = 1;
        goto quit;
    }

    our_sdl_event = SDL_RegisterEvents(1);

    if(sfconnection.init(our_sdl_event) != 0)
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

    sfconnection.start_thread();

    SDL_Event e;
    for(;;)
    {
        if(!have_focus)
        {
#if DEBUG
            cout << "sleeping" << endl;
#endif
            usleep(SLEEPTIME_NO_FOCUS_US);
        }

        while(SDL_WaitEventTimeout(&e, 16))
        {
            if(e.type == SDL_QUIT)
            {
                err = 0;
                goto quit;
            }

            if(e.type == our_sdl_event)
            {
                if(e.user.code == BUFFER)
                {
                    // sfconnection has a new buffer
                    native_handle_t *handle;
                    buffer_info_t *info;

                    handle = sfconnection.get_current_handle();
                    info = sfconnection.get_current_info();

                    int failed = renderer.render_buffer(handle, *info);
                    sfconnection.release_buffer(failed);
                }
                else if(e.user.code == NO_BUFFER)
                {
                    // dummy render to avoid unresponsive error
                    renderer.dummy_draw(sfconnection.get_current_info()->pixel_format);
                }
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
                        sfconnection.lost_focus();
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
                        printf("focus gained\n");
#endif
                        wakeup_android();
                        sfconnection.gained_focus();
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
    }

quit:
    uinput.deinit();
    renderer.deinit();
    sfconnection.stop_thread();
    sfconnection.deinit();
    rmdir(SFDROID_ROOT);
    return err;
}

