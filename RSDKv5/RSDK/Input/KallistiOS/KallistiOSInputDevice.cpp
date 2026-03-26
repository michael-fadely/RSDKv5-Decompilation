#include "KallistiOSInputDevice.hpp"

#include <dc/maple.h>
#include <dc/maple/controller.h>

#include <RSDK/Core/Stub.hpp>

#include "sonicmania.xbm"

void SKU::Vmu::FrameBuffer::Print(std::string str, unsigned lineSpacing, Rect rect) {
    vmufb_print_string_into(this, nullptr, rect.x, rect.y,
                            rect.w, rect.h, lineSpacing, str.c_str());
    changed = true;
}

void SKU::Vmu::FrameBuffer::Blit(const uint8_t* data, Rect rect) {
    vmufb_paint_area(this, rect.x, rect.y, rect.w, rect.h, data);
    changed = true;
}

void SKU::Vmu::FrameBuffer::Fill(bool value, Rect rect) {
    for(unsigned y = rect.y; y < rect.h; ++y) {
        for(unsigned x = rect.x; x < rect.w; ++x) {
            if(value) data[x] |= (1 << y);
            else data[x] &= ~(1 << y);
        }
    }
    changed = true;
}

void SKU::Vmu::FrameBuffer::DrawXBM(const uint8_t* data, int h) {
    vmufb_paint_xbm(this, 0, h, 48, 32-h, data);
    changed = true;
}

maple_device_t* SKU::Vmu::Dev() const {
    maple_device_t* device = maple_enum_dev(port, slot);
    return (device && (device->info.functions & MAPLE_FUNC_LCD))?
            device : nullptr;
}

vmu_state_t* SKU::Vmu::State() const {
    auto* d = Dev();

    if(d)
        return reinterpret_cast<vmu_state_t*>(maple_dev_status(d));
    return nullptr;
}

bool SKU::Vmu::Valid() const {
    return Dev() != nullptr;
}

bool SKU::Vmu::Beep(uint8_t tone, std::function<void(void)> task) const {
    auto* d = Dev();

    if(d)
        vmu_beep_waveform(d, tone, tone / 2, tone, tone / 2);

    task();

    if(d) {
        vmu_beep_waveform(d, 0, 0, 0, 0);
        return true;
    }

    return false;
}

bool SKU::Vmu::Pressed(SKU::Vmu::Button button) const {
    const auto* s = State();

    if(s)
        return (s->buttons.current.raw & static_cast<unsigned>(button));

    return false;
}

bool SKU::Vmu::Tapped(SKU::Vmu::Button button) const {
    const auto* s = State();

    if(s)
        return (s->buttons.previous.raw & static_cast<unsigned>(button));

    return false;
}

bool SKU::Vmu::Update() const {
    auto* d = Dev();

    if(fb.changed && d) {
        vmufb_present(&fb, d);
        fb.changed = false;
        return true;
    }

    return false;
}

void SKU::KallistiOSInputDevice::UpdateInput() {
    maple_device_t *device = maple_enum_dev(Port(), 0);
    if(device) {
        active = true;

        if(device->info.functions & MAPLE_FUNC_CONTROLLER) {
            gamepadType = (DEVICE_TYPE_CONTROLLER << 8);
        } else if(device->info.functions & MAPLE_FUNC_KEYBOARD) {
            gamepadType = (DEVICE_TYPE_KEYBOARD << 8);
        }

        for(unsigned v = 0; v < 2; ++v)
            vmu[v].Update();

        ProcessInput(!Port()? CONT_ANY : CONT_P1 + 1);
    } else {
        active = false;
        gamepadType = DEVICE_TYPE_NONE;
    }
}

void SKU::KallistiOSInputDevice::ProcessInput(int32 controllerID) {
    auto& retro = controller[controllerID];
    auto& leftAnalog = stickL[controllerID];
    auto& leftTrigger = triggerL[controllerID];
    auto& rightTrigger = triggerR[controllerID];

    auto* dev = maple_enum_dev(Port(), 0);
    assert(dev);

    switch((gamepadType >> 8)) {
        case DEVICE_TYPE_CONTROLLER: {
            auto& state = *reinterpret_cast<cont_state_t*>(maple_dev_status(dev));

            retro.keyUp.press |= (state.buttons & CONT_DPAD_UP) != 0;
            retro.keyDown.press |= (state.buttons & CONT_DPAD_DOWN) != 0;
            retro.keyLeft.press |= (state.buttons & CONT_DPAD_LEFT) != 0;
            retro.keyRight.press |= (state.buttons & CONT_DPAD_RIGHT) != 0;

            float fjoyx = (state.joyx / 128.0f);
            float fjoyy = (state.joyy / 128.0f);

            leftAnalog.keyUp.press |= (fjoyy <= -0.6f);
            leftAnalog.keyDown.press |= (fjoyy >= 0.6f);
            leftAnalog.keyLeft.press |= (fjoyx <= -0.6f);
            leftAnalog.keyRight.press |= (fjoyx >= 0.6f);

            retro.keyA.press |= (state.buttons & CONT_A) != 0;
            retro.keyB.press |= (state.buttons & CONT_B) != 0;
            retro.keyC.press |= (state.buttons & CONT_C) != 0;
            retro.keyC.press |= (vmu[0].Pressed(Vmu::Button::A)
                              || vmu[1].Pressed(Vmu::Button::A));

            retro.keyX.press |= (state.buttons & CONT_X) != 0;
            retro.keyY.press |= (state.buttons & CONT_Y) != 0;
            retro.keyZ.press |= (state.buttons & CONT_Z) != 0;
            retro.keyZ.press |= (vmu[0].Pressed(Vmu::Button::B)
                              || vmu[1].Pressed(Vmu::Button::B));

            leftTrigger.triggerDelta = static_cast<float>(CLAMP(state.ltrig, 0, 255)) / 255.0f;
            leftTrigger.keyTrigger.press = leftTrigger.triggerDelta > INPUT_DEADZONE;

            rightTrigger.triggerDelta = static_cast<float>(CLAMP(state.rtrig, 0, 255)) / 255.0f;
            rightTrigger.keyTrigger.press = rightTrigger.triggerDelta > INPUT_DEADZONE;

            retro.keyStart.press |= (state.buttons & CONT_START) != 0;
            retro.keySelect.press |= (state.buttons & CONT_D) != 0;
            retro.keySelect.press |= (vmu[0].Pressed(Vmu::Button::Mode)
                                   || vmu[1].Pressed(Vmu::Button::Mode));
            break;
        }
        case DEVICE_TYPE_KEYBOARD: {
            auto& kbd = *reinterpret_cast<kbd_state_t*>(maple_dev_status(dev));

            retro.keyUp.press |= kbd.key_states[KBD_KEY_UP].is_down;
            retro.keyDown.press |= kbd.key_states[KBD_KEY_DOWN].is_down;
            retro.keyLeft.press |= kbd.key_states[KBD_KEY_LEFT].is_down;
            retro.keyRight.press |= kbd.key_states[KBD_KEY_RIGHT].is_down;

            leftAnalog.keyUp.press |= retro.keyUp.press;
            leftAnalog.keyDown.press |= retro.keyDown.press;
            leftAnalog.keyLeft.press |= retro.keyRight.press;
            leftAnalog.keyRight.press |= retro.keyLeft.press;

            retro.keyA.press |= kbd.key_states[KBD_KEY_A].is_down;
            retro.keyB.press |= kbd.key_states[KBD_KEY_B].is_down;
            retro.keyC.press |= kbd.key_states[KBD_KEY_C].is_down;

            retro.keyX.press |= kbd.key_states[KBD_KEY_X].is_down;
            retro.keyY.press |= kbd.key_states[KBD_KEY_Y].is_down;
            retro.keyZ.press |= kbd.key_states[KBD_KEY_Z].is_down;

            retro.keyStart.press |= (kbd.key_states[KBD_KEY_S].is_down
                                  || kbd.key_states[KBD_KEY_ENTER].is_down);
            retro.keySelect.press |= kbd.key_states[KBD_KEY_D].is_down;
            retro.keySelect.press |= kbd.key_states[KBD_KEY_TAB].is_down;

            break;
        }
    }
}

void SKU::KallistiOSInputDevice::CloseDevice() {
    printHere();
}

namespace {
    SKU::KallistiOSInputDevice inputDevices[PLAYER_COUNT] = { 0, 1, 2, 3 };
}

void SKU::InitKallistiOSInputAPI() {
    // Enable KOS to poll for button input from any attached VMUs.
    vmu_set_buttons_enabled(true);

    inputDeviceCount = PLAYER_COUNT;
    for(unsigned c = 0; c < PLAYER_COUNT; ++c) {
        inputDevices[c].disabled = false;
        inputDevices[c].active = false;
        inputDevices[c].isAssigned = true;
        inputDevices[c].gamepadType = DEVICE_TYPE_NONE;

        inputSlots[c] = CONT_P1 + c;
        inputDeviceList[c] = &inputDevices[c];
        inputSlotDevices[c] = &inputDevices[c];

        inputDevices[c].vmu[0].fb.DrawXBM(manialogo_bits, 0);
    }
}
