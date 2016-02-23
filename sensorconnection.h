#ifndef __SENSORS_CONNECTION_H__
#define __SENSORS_CONNECTION_H__

#include "sfdroid_defs.h"

#include <sensormanagerinterface.h>
#include <accelerometersensor_i.h>

#include <thread>
#include <atomic>

class sensorconnection_t {
    public:
        sensorconnection_t() : fd_pass_socket(-1), fd_client(-1), remoteSensorManager(nullptr), accel(nullptr), running(false), have_focus(true) {}
        int init();
        void deinit();
        int wait_for_client();
        bool have_client();
        void update_timeout();
        int wait_for_request(int &type, int &timedout);
        int send_accelerometer_data();
        void start_thread();
        void thread_loop();
        void stop_thread();

        void lost_focus();
        void gained_focus();
    private:
        int fd_pass_socket; // listen for surfaceflinger
        int fd_client; // the client (sharebuffer module)

        SensorManagerInterface *remoteSensorManager;
        AccelerometerSensorChannelInterface *accel;

        std::thread my_thread;

        std::atomic<bool> running;

        bool have_focus;
};

#endif

