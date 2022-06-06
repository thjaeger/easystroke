#pragma once

#include <gtkmm.h>
#include <X11/extensions/XInput2.h>

#include "prefdb.h"

namespace Events {

    struct XiDevice {
        int dev;
        std::string name;
        bool absolute;
        bool active;
        int proximity_axis;
        double scale_x, scale_y;
        int num_buttons;
        int master;
        explicit XiDevice(XIDeviceInfo *info);

        bool inProximity(const XIValuatorState &valuators);
    };

    /**
     * Observes changes to devices (e.g. mouse/keyboard) and maintains a list of currently active devices.
     * It does not keep track of whether devices have buttons/keys "grabbed", that responsibility is in the DeviceGrabber
     */
    class DeviceObserver {
    private:

        void update_excluded();

        void new_device(XIDeviceInfo *);

    public:
        // TODO: Should be private
        int opcode, event, error;

        // TODO: Should be private
        std::map<XID, std::shared_ptr<Events::XiDevice> > xi_devs;

        DeviceObserver();

        Events::XiDevice *get_xi_dev(int id);

        bool hierarchy_changed(XIHierarchyEvent *);
    };
}
