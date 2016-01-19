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

using namespace std;

int main(int argc, char *argv[])
{
    int err = 0;

    native_handle_t *handle = NULL;
    buffer_info_t info;
    int timedout = 0;

    sfconnection_t sfconnection;
    renderer_t renderer;

#if DEBUG
    cout << "setting up sfdroid directory" << endl;
#endif
    mkdir(SFDROID_ROOT, 0770);
    touch(FOCUS_FILE);

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
                        cout << "focus lost" << endl;
#endif
                        unlink(FOCUS_FILE);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
#if DEBUG
                        cout << "focus gained" << endl;
#endif
                        touch(FOCUS_FILE);
#if DEBUG
                        cout << "waking up android" << endl;
#endif
                        wakeup_android();
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
    renderer.deinit();
    sfconnection.deinit();
    unlink(FOCUS_FILE);
    rmdir(SFDROID_ROOT);
    return err;
}

