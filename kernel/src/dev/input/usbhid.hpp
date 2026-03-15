#pragma once

#include <dev/input/input.hpp>
#include <klib/vector.hpp>

namespace dev::xhci { struct Device; struct Endpoint; }

namespace dev::input::usbhid {
    struct InputItem {
        u32 size_bits;
        // for variable type
        u16 type, code;
        // for array type
        u32 usage_min, usage_max;

        bool is_array, is_constant;

        i32 previous_value;
    };

    struct Device : virtual public input::InputDevice {
        xhci::Device *xhci_device;
        xhci::Endpoint *xhci_endpoint;

        klib::Vector<InputItem> input_items;
        bool report_protocol_usable = false;

        void parse_report_descriptor(u8 *data, u16 length);

        void event(u8 *data, u32 data_length);
        void process_button(usize index, bool state);
    };

    struct Mouse : public Device, public input::MouseDevice {
        Mouse();
    };

    struct Keyboard : public Device, public input::KeyboardDevice {
        Keyboard();
    };
}
