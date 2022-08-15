#pragma once

#include <atomic>

struct BladeRFDeviceState
{
    BladeRFDeviceState()
        : initState(false),
          captureState(false)
    {

    }

    std::atomic_bool initState;
    std::atomic_bool captureState;
};
