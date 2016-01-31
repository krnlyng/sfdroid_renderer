#ifndef __UINPUT_H__
#define __UINPUT_H__

#include <linux/uinput.h>

#define MAX_PRESSURE 5

class uinput_t
{
    public:
        uinput_t() : fd_uinput(-1), uinput_dev_created(0) { }
        int init(int win_width, int win_height);
        int send_event(int type, int code, int value);
        void deinit();
    private:
        int fd_uinput;
        int uinput_dev_created;
        struct uinput_user_dev uidev;
};

#endif
