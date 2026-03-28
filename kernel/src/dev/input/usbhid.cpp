#include <dev/input/usbhid.hpp>
#include <dev/xhci.hpp>
#include <klib/cstdio.hpp>

namespace dev::input::usbhid {
    constexpr u16 usb_to_linux_keycode_table[] = {
#include "usb_to_linux_keycodes.inc"
    };
    static_assert(sizeof(usb_to_linux_keycode_table) / sizeof(u16) == 256);

    void Device::parse_report_descriptor(u8 *data, u16 length) {
        // Global state
        u32 usage_page = 0;
        [[maybe_unused]] i32 logical_minimum = 0;
        [[maybe_unused]] i32 logical_maximum = 0;
        u32 report_size = 0;
        u32 report_count = 0;

        // Local state (they do not carry over to the next Main item)
        klib::Vector<u32> usages;
        u32 usage_minimum = 0, usage_maximum = 0;

        report_protocol_usable = true; // will be set to false if anything is invalid

        for (u16 i = 0; i < length; i++) {
            u8 item = data[i];
            u8 item_size = item & 0b11;
            u8 item_type = (item >> 2) & 0b11;
            u8 item_tag = (item >> 4) & 0b1111;
            u32 item_data = 0;

            if (item_size == 3)
                item_size = 4;

            for (u16 offset = 0; offset < item_size; offset++) {
                if (i + offset >= length) break;
                item_data |= (u32)data[i + offset + 1] << (offset * 8);
            }
            i += item_size;

            switch (item_type) {
            case 0: { // Main
                switch (item_tag) {
                case 8: { // Input
                    bool is_constant = (item_data >> 0) & 1;
                    bool is_array    = !((item_data >> 1) & 1);

                    klib::printf("USB-HID: Input item");
                    if (is_constant) klib::printf(", constant");
                    if (is_array) klib::printf(", array");
                    klib::printf("\nUSB-HID:     Usage page: %#X, Report size: %u, Report count: %u\n", usage_page, report_size, report_count);
                    if (usage_minimum != 0 || usage_maximum != 0)
                        klib::printf("USB-HID:     Usage minimum: %#X, Usage maximum: %#X\n", usage_minimum, usage_maximum);
                    if (usages.size() > 0) {
                        klib::printf("USB-HID:     Usages:");
                        for (usize i = 0; i < usages.size(); i++)
                            klib::printf(" %#X", usages[i]);
                        klib::printf("\n");
                    }

                    if (is_constant) {
                        InputItem input_item {
                            .size_bits = report_size * report_count,
                            .is_constant = true
                        };
                        this->input_items.push_back(input_item);
                        break;
                    }

                    if (is_array) {
                        for (u32 i = 0; i < report_count; i++) {
                            InputItem input_item {
                                .size_bits = report_size,
                                .usage_min = usage_minimum, .usage_max = usage_maximum,
                                .is_array = true
                            };
                            this->input_items.push_back(input_item);
                        }
                        break;
                    }

                    for (u32 i = 0; i < report_count; i++) {
                        u32 usage = 0;
                        if (i < usages.size()) {
                            usage = usages[i];
                        } else if (usages.size() != 0) {
                            usage = usages[usages.size() - 1];
                        } else if (usage_minimum != 0) {
                            usage = klib::min(usage_minimum + i, usage_maximum);
                        } else {
                            klib::printf("USB-HID: Invalid usage state for input item\n");
                            report_protocol_usable = false;
                            break;
                        }

                        u16 type = 0, code = 0;
                        if ((usage >> 16) == 7) {
                            type = EV_KEY;
                            code = usb_to_linux_keycode_table[usage & 0xFF];
                        } else {
                            switch (usage) {
                            case 0x90001: type = EV_KEY; code = BTN_LEFT; break;
                            case 0x90002: type = EV_KEY; code = BTN_RIGHT; break;
                            case 0x90003: type = EV_KEY; code = BTN_MIDDLE; break;
                            case 0x90004: type = EV_KEY; code = BTN_SIDE; break;
                            case 0x90005: type = EV_KEY; code = BTN_EXTRA; break;

                            case 0x10030: type = EV_REL; code = REL_X; break;
                            case 0x10031: type = EV_REL; code = REL_Y; break;
                            case 0x10038: type = EV_REL; code = REL_WHEEL; break;

                            default:
                                klib::printf("USB-HID: Unsupported usage %#X for input item\n", usage);
                                break;
                            }
                        }

                        InputItem input_item {
                            .size_bits = report_size,
                            .type = type, .code = code
                        };
                        this->input_items.push_back(input_item);
                    }
                } break;
                default:
                    klib::printf("USB-HID: Unsupported main item tag %u\n", item_tag);
                    break;
                }

                usages.clear();
                usage_minimum = 0;
                usage_maximum = 0;
            } break;
            case 1: { // Global
                switch (item_tag) {
                case 0: usage_page = item_data; break;
                case 1: logical_minimum = item_data; break;
                case 2: logical_maximum = item_data; break;
                case 7: report_size = item_data; break;
                case 9: report_count = item_data; break;
                default:
                    klib::printf("USB-HID: Unsupported global item tag %u\n", item_tag);
                    break;
                }
            } break;
            case 2: { // Local
                u32 usage = item_size == 4 ? item_data : item_data | (usage_page << 16);
                switch (item_tag) {
                case 0: usages.push_back(usage); break;
                case 1: usage_minimum = usage; break;
                case 2: usage_maximum = usage; break;
                default:
                    klib::printf("USB-HID: Unsupported local item tag %u\n", item_tag);
                    break;
                }
            } break;
            case 3: // Reserved
            default:
                klib::printf("USB-HID: Reserved type item found\n");
                report_protocol_usable = false;
                break;
            }
        }

        u32 bit_offset = 0;
        for (usize i = 0; i < this->input_items.size(); i++) {
            const auto &input_item = this->input_items[i];
            klib::printf("USB-HID: Input Item %2lu: %2u bits", i, input_item.size_bits);
            if (input_item.is_array)
                klib::printf(", array, usage min: %#X, usage max: %#X", input_item.usage_min, input_item.usage_max);
            else
                klib::printf(", variable, type: %#X, code: %#X", input_item.type, input_item.code);
            klib::printf("\n");
            if (input_item.type != 0) {
                if (input_item.size_bits != 1) {
                    if (bit_offset % 8 == 0) {
                        if (input_item.size_bits != 8 && input_item.size_bits != 16) {
                            klib::printf("USB-HID: Field with unsupported size in report (index: %lu, offset: %#X bits, field size: %u bits)\n", i, bit_offset, input_item.size_bits);
                            report_protocol_usable = false;
                        }
                    } else {
                        klib::printf("USB-HID: Field with unsupported alignment in report (index: %lu, offset: %#X bits, field size: %u bits)\n", i, bit_offset, input_item.size_bits);
                        report_protocol_usable = false;
                    }
                }
            }
            bit_offset += input_item.size_bits;
        }

        if (!report_protocol_usable) {
            klib::printf("USB-HID: Report protocol not usable, hoping boot protocol works\n");
            input_items.clear();
            return;
        }

        for (usize i = 0; i < this->input_items.size(); i++) {
            auto &input_item = this->input_items[i];

            if (input_item.is_array) {
                for (u32 usage = input_item.usage_min; usage <= input_item.usage_max; usage++) {
                    if ((usage >> 16) == 7) { // keyboard usage page
                        u16 keycode = usb_to_linux_keycode_table[usage & 0xFF];
                        key_bitmap.set(keycode, true);
                    }
                }
                continue;
            }

            if (input_item.type == 0)
                continue;

            ev_bitmap.set(input_item.type, true);
            switch (input_item.type) {
            case EV_KEY: key_bitmap.set(input_item.code, true); break;
            case EV_REL: rel_bitmap.set(input_item.code, true); break;
            default: ASSERT(false);
            }
        }
    }

    void Device::process_button(usize index, bool state) {
        bool old_state = key_status.get(index);
        if (state != old_state) {
            key_status.set(index, state);
            push_event(EV_KEY, index, state);
        }
    }

    void Device::event(u8 *data, u32 data_length) {
        // klib::printf("USB-HID: Dumping data:");
        // for (u16 i = 0; i < data_length; i++)
        //     klib::printf(" %02x", data[i]);
        // klib::printf("\n");

        u32 bit_offset = 0;
        for (usize i = 0; i < this->input_items.size(); i++) {
            auto &input_item = this->input_items[i];
            u32 byte_offset = bit_offset / 8;
            i32 value = 0;

            if (byte_offset >= data_length) break;

            if (input_item.size_bits == 1) {
                value = (data[byte_offset] & (1 << bit_offset)) != 0;
            } else if (bit_offset % 8 == 0) {
                if (input_item.size_bits == 8) {
                    value = (i8)data[byte_offset];
                } else if (input_item.size_bits == 16) {
                    if (byte_offset + 1 >= data_length) break;
                    value = (i16)((u16)data[byte_offset] | ((u16)data[byte_offset + 1] << 8));
                } else {
                    klib::unreachable(); // validated unreachable when parsing the report descriptor
                }
            }

            if (input_item.is_array) {
                u32 usage = input_item.usage_min + value;
                u32 old_usage = input_item.usage_min + input_item.previous_value;
                if ((usage >> 16) == 7) { // keyboard usage page
                    u16 keycode = usb_to_linux_keycode_table[usage & 0xFF];
                    u16 old_keycode = usb_to_linux_keycode_table[old_usage & 0xFF];
                    if (value != input_item.previous_value) {
                        key_status.set(keycode, 1);
                        key_status.set(old_keycode, 0);
                        push_event(EV_KEY, keycode, 1);
                        push_event(EV_KEY, old_keycode, 0);
                        input_item.previous_value = value;
                    }
                }
            } else if (!input_item.is_constant) {
                switch (input_item.type) {
                case EV_KEY: {
                    process_button(input_item.code, value);
                } break;
                case EV_REL: {
                    if (value != 0)
                        push_event(EV_REL, input_item.code, value);
                } break;
                default: break;
                }
            }

            bit_offset += input_item.size_bits;
        }
        flush_events();
    }

    Mouse::Mouse() {
        name = "USB Mouse";
        phys = "usbmouse/input0";
        id = { .bustype = BUS_USB, .vendor = 2, .product = 1, .version = 1 };
        ev_bitmap.set(EV_SYN, true);
    }

    Keyboard::Keyboard() {
        name = "USB Keyboard";
        phys = "usbkbd/input0";
        id = { .bustype = BUS_USB, .vendor = 1, .product = 1, .version = 1 };
        ev_bitmap.set(EV_SYN, true);
    }
}
