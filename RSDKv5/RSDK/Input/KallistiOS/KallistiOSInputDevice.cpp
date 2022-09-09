#include "KallistiOSInputDevice.hpp"

#include <dc/maple.h>
#include <dc/maple/controller.h>

#include <RSDK/Core/Stub.hpp>

void SKU::KallistiOSInputDevice::UpdateInput() {
    maple_device_t* mapleDevice = maple_enum_type(this->controllerID, MAPLE_FUNC_CONTROLLER);

    if (!mapleDevice) {
        return;
    }

    cont_state_t* mapleState = (cont_state_t*)maple_dev_status(mapleDevice);

    if (!mapleState) {
        return;
    }

    this->state = *mapleState;
    this->ProcessInput(CONT_ANY);
}

void SKU::KallistiOSInputDevice::ProcessInput(int32 controllerID) {
    auto& retro = controller[controllerID];

    retro.keyUp.press |= (state.buttons & CONT_DPAD_UP) != 0;
    retro.keyDown.press |= (state.buttons & CONT_DPAD_DOWN) != 0;
    retro.keyLeft.press |= (state.buttons & CONT_DPAD_LEFT) != 0;
    retro.keyRight.press |= (state.buttons & CONT_DPAD_RIGHT) != 0;

    retro.keyA.press |= (state.buttons & CONT_A) != 0;
    retro.keyB.press |= (state.buttons & CONT_B) != 0;
    retro.keyC.press |= (state.buttons & CONT_C) != 0;
    retro.keyX.press |= (state.buttons & CONT_X) != 0;
    retro.keyY.press |= (state.buttons & CONT_Y) != 0;
    retro.keyZ.press |= (state.buttons & CONT_Z) != 0;

    retro.keyStart.press |= (state.buttons & CONT_START) != 0;
    retro.keySelect.press |= (state.buttons & CONT_D) != 0;
}

void SKU::KallistiOSInputDevice::CloseDevice() {
    printHere();
}

void SKU::InitKallistiOSInputAPI() {
    printHere();
    for (int32 i = 0; i < PLAYER_COUNT; ++i) {
        KallistiOSInputDevice* kosInputDevice = nullptr;

        for (int32 d = 0; d < inputDeviceCount; ++d) {
            if (inputDeviceList[d] && inputDeviceList[d]->id == i) {
                kosInputDevice = (KallistiOSInputDevice*)inputDeviceList[d];
            }
        }

        maple_device_t* mapleDevice = maple_enum_type(i, MAPLE_FUNC_CONTROLLER);

        if (mapleDevice == nullptr) {
            if (kosInputDevice) {
                RemoveInputDevice(kosInputDevice);
            }

            continue;
        }

        cont_state_t* state = (cont_state_t*)maple_dev_status(mapleDevice);

        if (state == nullptr) {
            if (kosInputDevice) {
                RemoveInputDevice(kosInputDevice);
            }

            continue;
        }

        if (!kosInputDevice) {
            if (inputDeviceCount == INPUTDEVICE_COUNT) {
                continue;
            }

            if (inputDeviceList[inputDeviceCount] && inputDeviceList[inputDeviceCount]->active) {
                continue;
            }

            if (inputDeviceList[inputDeviceCount]) {
                delete inputDeviceList[inputDeviceCount];
            }

            kosInputDevice = new KallistiOSInputDevice();
            inputDeviceList[inputDeviceCount] = kosInputDevice;
            kosInputDevice->disabled = false;
            kosInputDevice->id = i;
            kosInputDevice->active = true;

            for (int32 p = 0; p < PLAYER_COUNT; ++p) {
                if (inputSlots[p] == i) {
                    inputSlotDevices[p] = kosInputDevice;
                    kosInputDevice->isAssigned  = true;
                }
            }

            inputDeviceCount++;
        }

        kosInputDevice->controllerID = i;
    }
}
