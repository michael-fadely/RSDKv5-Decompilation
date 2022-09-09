#ifndef RSDKV5_KALLISTIOSINPUTDEVICE_H
#define RSDKV5_KALLISTIOSINPUTDEVICE_H

#include <dc/maple/controller.h>

namespace SKU
{

struct KallistiOSInputDevice : InputDevice
{
    void UpdateInput() override;
    void ProcessInput(int32 controllerID) override;
    void CloseDevice() override;
    cont_state_t state {};
    int32 mapleIndex = 0;
};

void InitKallistiOSInputAPI();

}

#endif // RSDKV5_KALLISTIOSINPUTDEVICE_H
