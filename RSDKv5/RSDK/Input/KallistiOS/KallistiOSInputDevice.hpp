#ifndef RSDKV5_KALLISTIOSINPUTDEVICE_H
#define RSDKV5_KALLISTIOSINPUTDEVICE_H

#include <dc/maple/controller.h>
#include <dc/maple/vmu.h>
#include <dc/vmu_fb.h>

namespace SKU
{

class Vmu {
private:
    maple_device_t* Dev() const;
    vmu_state_t* State() const;
public:
    struct FrameBuffer: vmufb_t {
        struct Rect {
            Rect() {}

            unsigned x = 0;
            unsigned y = 0;
            unsigned w = VMU_SCREEN_WIDTH;
            unsigned h = VMU_SCREEN_HEIGHT;
        };

        mutable bool changed = false;

        void Print(std::string str, unsigned lineSpacing=2, Rect rect=Rect());
        void Blit(const uint8_t* data, Rect rect=Rect());
        void Fill(bool value=true, Rect rect=Rect());
    } fb;

    enum Button: uint8_t {
        Up    = VMU_DPAD_UP,
        Down  = VMU_DPAD_DOWN,
        Left  = VMU_DPAD_LEFT,
        Right = VMU_DPAD_RIGHT,
        A     = VMU_A,
        B     = VMU_B,
        Mode  = VMU_MODE,
        Sleep = VMU_SLEEP
    };

    const uint8_t port;
    const uint8_t slot;

    Vmu(uint8_t port_, uint8_t slot_):
        port(port_), slot(slot_) {}

    bool Valid() const;
    bool Update() const;
    bool Beep(uint8_t tone=255, ::std::function<void(void)> task={}) const;
    bool Pressed(Button button) const;
    bool Tapped(Button button) const;
};

struct KallistiOSInputDevice : InputDevice
{
    std::array<Vmu, 2> vmu;

    KallistiOSInputDevice(int port):
        vmu({ Vmu{ port, 1 }, Vmu{ port, 2 }})
    {
        id = port + CONT_P1;
    }

    void UpdateInput() override;
    void ProcessInput(int32 controllerID) override;
    void CloseDevice() override;
    int Port() const { return id - CONT_P1; }
};

void InitKallistiOSInputAPI();

}

#endif // RSDKV5_KALLISTIOSINPUTDEVICE_H
