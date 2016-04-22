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
#include "appconnection.h"

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

bool is_blacklisted(string app)
{
    if(app == "com.cyanogenmod.trebuchet")
    {
        return true;
    }
    else if(app == "com.android.systemui")
    {
        return true;
    }
    return false;
}

void handle_input_events(SDL_Event &e, uinput_t &uinput, vector<int> &slot_to_fingerId, int width, int height, int swipe_hack_dist_x, int swipe_hack_dist_y)
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
                if(e.tfinger.x >= width - swipe_hack_dist_x)
                {
#if DEBUG
                    cout << "swipe hack x" << endl;
#endif
                    x = width;
                }
                if(e.tfinger.y <= swipe_hack_dist_y)
                {
#if DEBUG
                    cout << "swipe hack y" << endl;
#endif
                    y = 0;
                }
                else y = e.tfinger.y;
                if(e.tfinger.y >= height - swipe_hack_dist_y)
                {
#if DEBUG
                    cout << "swipe hack y" << endl;
#endif
                    y = height;
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

int handle_buffer_event(SDL_Event &e, sfconnection_t &sfconnection, renderer_t *renderer, map<string, renderer_t*> &windows, unsigned int &last_time, unsigned int &current_time, int &frames)
{
    int failed = 1;

    if(e.user.code == BUFFER)
    {
        // sfconnection has a new buffer
        ANativeWindowBuffer *buffer;
        buffer_info_t *info;

        buffer = sfconnection.get_current_buffer();
        info = sfconnection.get_current_info();

        if(renderer->is_active())
        {
            failed = renderer->render_buffer(buffer, *info);
        }
        else
        {
            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
            {
                if(it->second->is_active())
                {
                    failed = it->second->render_buffer(buffer, *info);
                }
            }
        }

        frames++;
    }
    else if(e.user.code == NO_BUFFER)
    {
#if DEBUG
        cout << "no buffer received timeout" << endl;
#endif
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
        if(renderer->is_active())
        {
            failed = renderer->render_buffer(buffer, *info);
        }
        else
        {
            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
            {
                if(it->second->is_active())
                {
                    failed = it->second->render_buffer(buffer, *info);
                }
            }
        }

        frames++;
    }

    current_time = SDL_GetTicks();
    if(current_time > last_time + 1000)
    {
        cout << "Frames: " << frames << endl;
        last_time = current_time;
        frames = 0;
    }

    return failed;
}

bool handle_window_event(SDL_Event &e, sfconnection_t &sfconnection, renderer_t *renderer, map<string, renderer_t*> &windows, sensorconnection_t &sensorconnection, int &have_focus, std::string &last_appandactivity, bool &renderer_was_last, list<renderer_t*> &windows_to_delete)
{
#if DEBUG
    cout << "window event: " << (int)e.window.event << endl;
#endif
    switch(e.window.event)
    {
        case SDL_WINDOWEVENT_LEAVE:
        case SDL_WINDOWEVENT_FOCUS_LOST:
#if DEBUG
            cout << "focus lost" << endl;
#endif
            have_focus = 0;
            sfconnection.lost_focus();
            sensorconnection.lost_focus();

            if(renderer->get_window_id() == e.window.windowID)
            {
                if(renderer->is_active()) renderer->lost_focus();
            }
            else
            {
                for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                {
                    if(it->second->get_window_id() == e.window.windowID)
                    {
                        if(it->second->is_active()) it->second->lost_focus();
                        break;
                    }
                }
            }
            break;
        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
            cout << "focus gained" << endl;
#endif
            wakeup_android();
            sfconnection.gained_focus();
            sensorconnection.gained_focus();
            have_focus = 1;
            if(renderer->get_window_id() == e.window.windowID)
            {
                if(!renderer_was_last)
                {
                    go_home();
                }
                renderer_was_last = true;
                renderer->gained_focus();
            }
            else
            {
                for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                {
                    if(it->second->get_window_id() == e.window.windowID)
                    {
                        if(it->first + "/" + it->second->get_activity() != last_appandactivity || renderer_was_last)
                        {
                            start_app((it->first + "/" + it->second->get_activity()).c_str());
                        }
                        last_appandactivity = it->first + "/" + it->second->get_activity();
                        renderer_was_last = false;
                        it->second->gained_focus();
                        break;
                    }
                }
            }
            for(list<renderer_t*>::iterator it=windows_to_delete.begin();it!=windows_to_delete.end();it++)
            {
#if DEBUG
                cout << "deleting old window\n" << endl;
#endif
                delete *it;
            }
            windows_to_delete.clear();
            break;
        case SDL_WINDOWEVENT_CLOSE:
#if DEBUG
            cout << "window closed" << endl;
#endif
            if(renderer->get_window_id() == e.window.windowID)
            {
                return true;
            }
            else
            {
                for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
                {
                    if(it->second->get_window_id() == e.window.windowID)
                    {
                        stop_app(it->first.c_str());
                        it->second->deinit();
                        windows_to_delete.push_back(it->second);
                        windows.erase(it);
                        break;
                    }
                }
            }
            break;
    }
    return false;
}

void handle_app_event(SDL_Event &e, renderer_t *&renderer, map<string, renderer_t*> &windows, appconnection_t &appconnection, std::string &last_appandactivity, bool &renderer_was_last, list<renderer_t*> &windows_to_delete)
{
#if DEBUG
    cout << "received app event" << endl;
#endif
    string appandactivity, app, activity;
    string::size_type pos;
    appandactivity = appconnection.get_new_window();
    pos = appandactivity.find("/");

    if(pos != std::string::npos)
    {
        app = appandactivity.substr(0, pos);
        activity = appandactivity.substr(pos + 1);
    }
    else
    {
        app = appandactivity;
        activity = "";
    }

    if(!is_blacklisted(app))
    {
#if DEBUG
        cout << "new app or closing: " << app << endl;
#endif
        // make other windows loose focus.
        if(renderer->is_active())
        {
            renderer->lost_focus();
        }
        else
        {
            for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
            {
                if(app != it->first)
                {
                    if(it->second->is_active())
                    {
                        it->second->lost_focus();
                        break;
                    }
                }
            }
        }

        map<string, renderer_t*>::iterator the_window = windows.find(app);

        bool is_active = false;
        if(the_window != windows.end())
        {
            if(the_window->second != nullptr)
            {
                if(!the_window->second->is_active())
                {
                    the_window->second->deinit();
                    delete the_window->second;
                    windows.erase(the_window);
                }
                else if(e.user.code == CLOSE_APP)
                {
                    stop_app(app.c_str());

                    if(the_window->second->is_active()) the_window->second->lost_focus();
                    the_window->second->deinit();

                    // crash avoidance
                    windows_to_delete.push_back(the_window->second);
                    windows.erase(the_window);
                }
                else
                {
                    is_active = true;
                }
            }
        }

        if(e.user.code != CLOSE_APP)
        {
            if(!is_active)
            {
                windows[app] = new renderer_t();
                windows[app]->init();

                renderer_was_last = false;
                windows[app]->gained_focus();
                windows[app]->set_activity(activity);
                last_appandactivity = app + "/" + activity;
            }
        }
    }
}

void usage(const char *name)
{
    cout << name << " [--multiwindow|-m]" << endl;
    cout << "\t--multiwindow|-m android apps get their own windows" << endl;
}

int main(int argc, char *argv[])
{
    int err = 0;
    int swipe_hack_dist_x = 0;
    int swipe_hack_dist_y = 0;
    bool multiwindow = false;

    std::vector<SDL_Event> postponed_events;
    SDL_Event e;

    int have_focus = 1;

    sfconnection_t sfconnection;
    renderer_t *renderer = new renderer_t();
    map<string, renderer_t*> windows;
    list<renderer_t*> windows_to_delete;
    appconnection_t appconnection;
    std::string last_appandactivity = "";
    bool renderer_was_last = false;

    sensorconnection_t sensorconnection;

    uinput_t uinput;
    vector<int> slot_to_fingerId;

    uint32_t buffer_sdl_event = 0, app_sdl_event = 0;

    unsigned int last_time = 0, current_time = 0;
    int frames = 0;

    if(argc == 2)
    {
        if((std::string)argv[1] == "--multiwindow" || (std::string)argv[1] == "-m")
        {
            multiwindow = true;
        }
        else
        {
            cout << "invalid argument" << endl;
            usage(argv[0]);
            return 8;
        }
    }
    else if(argc > 2)
    {
        cout << "too many arguments" << endl;
        usage(argv[0]);
        return 7;
    }

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

    if(renderer->init() != 0)
    {
        err = 1;
        goto quit;
    }
    renderer->gained_focus();

    buffer_sdl_event = SDL_RegisterEvents(1);
    app_sdl_event = SDL_RegisterEvents(1);

    if(multiwindow) if(appconnection.init(app_sdl_event) != 0)
    {
        err = 1;
        goto quit;
    }

    swipe_hack_dist_x = (SWIPE_HACK_PIXEL_PERCENT * renderer->get_width()) / 100;
    swipe_hack_dist_y = (SWIPE_HACK_PIXEL_PERCENT * renderer->get_height()) / 100;

#if DEBUG
    cout << "swipe hack dist (x,y): (" << swipe_hack_dist_x << "," << swipe_hack_dist_y << ")" << endl;
#endif

    if(sfconnection.init(buffer_sdl_event) != 0)
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
    if(uinput.init(renderer->get_width(), renderer->get_height()) != 0)
    {
        err = 5;
        goto quit;
    }

    sfconnection.start_thread();
    if(multiwindow)
    {
        appconnection.start_thread();
    }
    sensorconnection.start_thread();

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
            else if(e.type == buffer_sdl_event)
            {
                int failed = handle_buffer_event(e, sfconnection, renderer, windows, last_time, current_time, frames);

                for(std::vector<SDL_Event>::iterator eit=postponed_events.begin();eit!=postponed_events.end();eit++)
                {
                    if(eit->type == SDL_WINDOWEVENT) if(handle_window_event(*eit, sfconnection, renderer, windows, sensorconnection, have_focus, last_appandactivity, renderer_was_last, windows_to_delete))
                    {
                        err = 0;
                        sfconnection.notify_buffer_done(1);
                        goto quit;
                    }
                    if(multiwindow) if(eit->type == app_sdl_event) handle_app_event(*eit, renderer, windows, appconnection, last_appandactivity, renderer_was_last, windows_to_delete);
                }
                postponed_events.resize(0);

                sfconnection.notify_buffer_done(failed);
            }
            else if(e.type == app_sdl_event)
            {
#if DEBUG
                cout << "delaying app event" << endl;
#endif
                postponed_events.push_back(e);
            }
            else if(e.type == SDL_WINDOWEVENT)
            {
                if(!have_focus)
                {
#if DEBUG
                    cout << "processing window event now" << endl;
#endif
                    if(handle_window_event(e, sfconnection, renderer, windows, sensorconnection, have_focus, last_appandactivity, renderer_was_last, windows_to_delete))
                    {
                        err = 0;
                        goto quit;
                    }
                }
                else
                {
#if DEBUG
                    cout << "delaying window event" << endl;
#endif
                    postponed_events.push_back(e);
                }
            }
            else if(have_focus)
            {
                handle_input_events(e, uinput, slot_to_fingerId, renderer->get_width(), renderer->get_height(), swipe_hack_dist_x, swipe_hack_dist_y);
            }
        }
    }

quit:
    uinput.deinit();
    if(multiwindow)
    {
        appconnection.stop_thread();
        appconnection.deinit();
    }
    renderer->deinit();
    delete renderer;
    for(map<string, renderer_t*>::iterator it=windows.begin();it!=windows.end();it++)
    {
        stop_app(it->first.c_str());
        it->second->deinit();
        delete it->second;
    }
    for(list<renderer_t*>::iterator it=windows_to_delete.begin();it!=windows_to_delete.end();it++)
    {
        delete *it;
    }
    windows_to_delete.clear();
    sensorconnection.stop_thread();
    sensorconnection.deinit();
    sfconnection.stop_thread();
    sfconnection.deinit();
    SDL_Quit();
    rmdir(SFDROID_ROOT);
    return err;
}

