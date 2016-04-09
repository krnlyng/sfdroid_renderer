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
#include <vector>
#include <algorithm>

#include <SDL.h>

#include <sys/stat.h>

#include "renderer.h"
#include "sfconnection.h"
#include "sensorconnection.h"
#include "utility.h"
#include "uinput.h"

using namespace std;

int find_slot(vector<int> &slot_to_fingerId, int fingerId)
{
    // find the slot
    vector<int>::iterator it = find(slot_to_fingerId.begin(), slot_to_fingerId.end(), fingerId);
    if(it != slot_to_fingerId.end())
    {
        return std::distance(slot_to_fingerId.begin(), it);
    }

    // find first free slot
    vector<int>::size_type i;
    for(i = 0;i < slot_to_fingerId.size();i++)
    {
        if(slot_to_fingerId[i] == -1)
        {
            slot_to_fingerId[i] = fingerId;
            return i;
        }
    }

    // no free slot found, add new one
    slot_to_fingerId.resize(slot_to_fingerId.size()+1);
    slot_to_fingerId[slot_to_fingerId.size()-1] = fingerId;

    return slot_to_fingerId.size()-1;
}

void erase_slot(vector<int> &slot_to_fingerId, int fingerId)
{
    vector<int>::iterator it = find(slot_to_fingerId.begin(), slot_to_fingerId.end(), fingerId);

    if(it != slot_to_fingerId.end())
    {
        vector<int>::size_type idx = distance(slot_to_fingerId.begin(), it);

        slot_to_fingerId[idx] = -1;

        // if we're at the end of the vector, erase unneeded elements
        if(idx == slot_to_fingerId.size()-1)
        {
            while(slot_to_fingerId[idx] == -1)
            {
                slot_to_fingerId.resize(idx);
                idx--;
            }
        }
    }
    else
    {
        cerr << "BUG: erase_slot" << endl;
    }
}

int main(int argc, char *argv[])
{
    int err = 0;
    int swipe_hack_dist_x = 0;
    int swipe_hack_dist_y = 0;

    int have_focus = 1;

    sfconnection_t sfconnection;
    sensorconnection_t sensorconnection;
    renderer_t renderer;
    map<string, renderer_t*> windows;
    uinput_t uinput;
    vector<int> slot_to_fingerId;

    uint32_t our_sdl_event = 0;

    unsigned int last_time = 0, current_time = 0;
    int frames = 0;

#if DEBUG
    cout << "setting up sfdroid directory" << endl;
#endif
    mkdir(SFDROID_ROOT, 0770);

#if DEBUG
    cout << "initializing SDL" << endl;
#endif
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        cerr << "SDL_Init failed" << endl;
        err = 1;
        goto quit;
    }

    windows["asd.vector.sensor/.MainActivity"] = new renderer_t();
    windows["asd.vector.sensor/.MainActivity"]->init();

    if(renderer.init() != 0)
    {
        err = 1;
        goto quit;
    }
    renderer.gained_focus();

    swipe_hack_dist_x = (SWIPE_HACK_PIXEL_PERCENT * renderer.get_width()) / 100;
    swipe_hack_dist_y = (SWIPE_HACK_PIXEL_PERCENT * renderer.get_height()) / 100;

#if DEBUG
    cout << "swipe hack dist (x,y): (" << swipe_hack_dist_x << "," << swipe_hack_dist_y << ")" << endl;
#endif

    our_sdl_event = SDL_RegisterEvents(1);

    if(sfconnection.init(our_sdl_event) != 0)
    {
        err = 2;
        goto quit;
    }

    if(sensorconnection.init() != 0)
    {
        err = 6;
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
    sensorconnection.start_thread();

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
                    ANativeWindowBuffer *buffer;
                    buffer_info_t *info;

                    buffer = sfconnection.get_current_buffer();
                    info = sfconnection.get_current_info();

                    int failed = 1;
                    if(renderer.is_active())
                    {
                        failed = renderer.render_buffer(buffer, *info);
                        static int hack = 0;
                        if(!hack)
                        {
                            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                            {
                                failed = it->second->render_buffer(buffer, *info);
                                hack = 1;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                        {
                            if(it->second->is_active())
                            {
                                failed = it->second->render_buffer(buffer, *info);
                                break;
                            }
                        }
                    }
                    sfconnection.notify_buffer_done(failed);

                    frames++;
                }
                else if(e.user.code == NO_BUFFER)
                {
                    ANativeWindowBuffer *buffer;
                    buffer_info_t *info;

                    buffer = sfconnection.get_current_buffer();
                    info = sfconnection.get_current_info();

                    // dummy draw to avoid unresponive message
                    // should be fine as long as surfaceflinger has at least 2 buffers
                    // because if no buffer is sent surfaceflinger should only be modifiying
                    // the other buffers and not the one that was just sent.
                    // we cannot save the screen anymore with the new rendering method therefore
                    // this method.
                    int failed = 1;
                    if(renderer.is_active())
                    {
                        failed = renderer.render_buffer(buffer, *info);
                    }
                    else
                    {
                        for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                        {
                            if(it->second->is_active())
                            {
                                failed = it->second->render_buffer(buffer, *info);
                                break;
                            }
                        }
                    }
                    sfconnection.notify_buffer_done(failed);

                    frames++;
                }

                current_time = SDL_GetTicks();
                if(current_time > last_time + 1000)
                {
                    cout << "Frames: " << frames << endl;
                    last_time = current_time;
                    frames = 0;
                }
            }

            if(e.type == SDL_WINDOWEVENT)
            {
                switch(e.window.event)
                {
                    case SDL_WINDOWEVENT_FOCUS_LOST:
#if DEBUG
                        cout << "focus lost" << endl;
#endif
                        have_focus = 0;
                        sfconnection.lost_focus();
                        sensorconnection.lost_focus();

                        if(renderer.get_window_id() == e.window.windowID)
                        {
                            renderer.lost_focus();
                        }
                        else
                        {
                            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                            {
                                if(it->second->get_window_id() == e.window.windowID)
                                {
                                    it->second->lost_focus();
                                    break;
                                }
                            }
                        }
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
                        cout << "focus gained" << endl;
#endif
                        wakeup_android();
                        sfconnection.gained_focus();
                        sensorconnection.gained_focus();
                        have_focus = 1;
                        if(renderer.get_window_id() == e.window.windowID)
                        {
                            go_home();
                            renderer.gained_focus();
                        }
                        else
                        {
                            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                            {
                                if(it->second->get_window_id() == e.window.windowID)
                                {
                                    start_app(it->first.c_str());
                                    it->second->gained_focus();
                                    break;
                                }
                            }
                        }
                        break;
                    case SDL_WINDOWEVENT_CLOSE:
#if DEBUG
                        cout << "window closed" << endl;
#endif
                        if(renderer.get_window_id() == e.window.windowID)
                        {
                            err = 0;
                            goto quit;
                        }
                        else
                        {
                            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                            {
                                if(it->second->get_window_id() == e.window.windowID)
                                {
                                    stop_app(it->first.c_str());
                                    it->second->deinit();
                                    windows.erase(it);
                                    break;
                                }
                            }
                        }
                        break;
                }
            }

            if(have_focus)
            {
                int x, y;
                int slot;
                switch(e.type)
                {
                    case SDL_FINGERUP:
#if DEBUG
                        cout << "SDL_FINGERUP" << endl;
#endif
                        slot = find_slot(slot_to_fingerId, e.tfinger.fingerId);

                        uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
                        uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, -1);
                        uinput.send_event(EV_SYN, SYN_REPORT, 0);

                        erase_slot(slot_to_fingerId, e.tfinger.fingerId);
                        break;
                    case SDL_FINGERMOTION:
#if DEBUG
                        cout << "SDL_FINGERMOTION" << endl;
#endif
                        slot = find_slot(slot_to_fingerId, e.tfinger.fingerId);

                        uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
                        uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, e.tfinger.fingerId);
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_X, e.tfinger.x);
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_Y, e.tfinger.y);
                        uinput.send_event(EV_ABS, ABS_MT_PRESSURE, e.tfinger.pressure * MAX_PRESSURE);
                        uinput.send_event(EV_SYN, SYN_REPORT, 0);
                        break;
                    case SDL_FINGERDOWN:
#if DEBUG
                        cout << "SDL_FINGERDOWN" << endl;
#endif
                        slot = find_slot(slot_to_fingerId, e.tfinger.fingerId);

                        uinput.send_event(EV_ABS, ABS_MT_SLOT, slot);
                        uinput.send_event(EV_ABS, ABS_MT_TRACKING_ID, e.tfinger.fingerId);
                        if(e.tfinger.x <= swipe_hack_dist_x)
                        {
#if DEBUG
                            cout << "swipe hack x" << endl;
#endif
                            x = 0;
                        }
                        else x = e.tfinger.x;
                        if(e.tfinger.x >= renderer.get_width() - swipe_hack_dist_x)
                        {
#if DEBUG
                            cout << "swipe hack x" << endl;
#endif
                            x = renderer.get_width();
                        }
                        if(e.tfinger.y <= swipe_hack_dist_y)
                        {
#if DEBUG
                            cout << "swipe hack y" << endl;
#endif
                            y = 0;
                        }
                        else y = e.tfinger.y;
                        if(e.tfinger.y >= renderer.get_height() - swipe_hack_dist_y)
                        {
#if DEBUG
                            cout << "swipe hack y" << endl;
#endif
                            y = renderer.get_height();
                        }
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_X, x);
                        uinput.send_event(EV_ABS, ABS_MT_POSITION_Y, y);
                        uinput.send_event(EV_ABS, ABS_MT_PRESSURE, e.tfinger.pressure * MAX_PRESSURE);
                        uinput.send_event(EV_SYN, SYN_REPORT, 0);
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
    for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
    {
        stop_app(it->first.c_str());
        it->second->deinit();
    }
    sensorconnection.stop_thread();
    sensorconnection.deinit();
    sfconnection.stop_thread();
    sfconnection.deinit();
    SDL_Quit();
    rmdir(SFDROID_ROOT);
    return err;
}

