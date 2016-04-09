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

#include "utility.h"

#include <fstream>
#include <unistd.h>
#include <cstring>

using namespace std;

void touch(const char *fname)
{
    fstream f;
    f.open(fname, ios::out);
}

void wakeup_android()
{
    system("/usr/bin/sfdroid_powerup &");
}

void start_app(const char *appandactivity)
{
    char buff[5120];
    snprintf(buff, 5120, "/usr/bin/am start --user 0 -n %s &", appandactivity);
    system(buff);
}

void go_home()
{
    system("/usr/bin/am start --user 0 -c android.intent.category.HOME -a android.intent.action.MAIN &");
}

void stop_app(const char *appandactivity)
{
    char app[5120];
    char buff[5120];
    strncpy(app, appandactivity, 5120);
    char *slash = strstr(app, "/");
    if(slash != NULL) *slash = 0;
    snprintf(buff, 5120, "/usr/bin/am force-stop --user 0 %s &", app);
    system(buff);
}

