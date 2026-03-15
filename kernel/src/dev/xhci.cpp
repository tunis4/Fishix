#include <dev/xhci.hpp>
#include <dev/io_service.hpp>
#include <cpu/interrupts/interrupts.hpp>
#include <sched/timer/hpet.hpp>
#include <klib/cstring.hpp>
#include <klib/cstdio.hpp>
#include <errno.h>

namespace dev::xhci {
    isize Controller::init() {
        bar = pci_device->get_bar(0);

        cap_regs.bar = bar;      cap_regs.bar_offset = 0;
        op_regs.bar = bar;       op_regs.bar_offset = bar->read<u8>(0);
        runtime_regs.bar = bar;  runtime_regs.bar_offset = cap_regs.read<u32>(24) & ~0b11111;
        doorbell_regs.bar = bar; doorbell_regs.bar_offset = cap_regs.read<u32>(20) & ~0b11;

        u32 hcsparams1 = cap_regs.read<u32>(4);
        u32 hcsparams2 = cap_regs.read<u32>(8);
        u32 hccparams1 = cap_regs.read<u32>(16);

        max_device_slots = (hcsparams1) & 0xFF;
        max_interrupters = (hcsparams1 >> 8) & 0x7FF;
        max_ports        = (hcsparams1 >> 24) & 0xFF;

        max_scratchpad_buffers = (((hcsparams2 >> 21) & 0x1F) << 5) | ((hcsparams2 >> 27) & 0x1F);

        context_size_64_bytes        = (hccparams1 >> 2) & 0x1;
        extended_capabilities_offset = ((hccparams1 >> 16) & 0xFFFF) * sizeof(u32);

        devices.resize(max_device_slots);
        for (u8 i = 0; i < devices.size(); i++)
            devices[i] = nullptr;

        ports.resize(max_ports);
        for (u8 i = 0; i < ports.size(); i++) {
            ports[i] = {};
            ports[i].index = i;
        }

        u8 ports_usb2 = 0, ports_usb3 = 0;
        iterate_extended_capabilities([&] (u32 cap_ptr, u32 dword0, u8 cap_id) {
            klib::printf("XHCI: Capability id: %d\n", cap_id);

            switch (cap_id) {
            case 1: { // USB Legacy Support
                klib::printf("XHCI:     Found legacy support data: %#X\n", dword0);
                if ((dword0 & (1 << 24)) && !(dword0 & (1 << 16))) {
                    klib::printf("XHCI:     Controller already released by BIOS\n");
                    return;
                }
                cap_regs.set_bit<u32>(cap_ptr, (1 << 24)); // ask bios to release ownership

                u32 timeout = 50;
                while (cap_regs.read<u32>(cap_ptr) & (1 << 16)) {
                    if (--timeout == 0) {
                        klib::printf("XHCI:     BIOS did not release ownership in 50 ms, forcing takeover\n");
                        cap_regs.clear_bit<u32>(cap_ptr, (1 << 16));
                        return;
                    }
                    sched::timer::hpet::stall_ms(1);
                }
                klib::printf("XHCI:     Controller ownership successfully taken from BIOS\n");
            } return;
            case 2: { // Supported Protocol
                u8 minor_revision = (dword0 >> 16) & 0xFF;
                u8 major_revision = (dword0 >> 24) & 0xFF;
                u32 name_string = cap_regs.read<u32>(cap_ptr + 4);
                if (memcmp(&name_string, "USB ", 4) != 0)
                    return;
                u32 dword2 = cap_regs.read<u32>(cap_ptr + 8);
                u8 port_offset = ((dword2 >> 0) & 0xFF) - 1;
                u8 port_count = (dword2 >> 8) & 0xFF;
                u16 port_flags = (dword2 >> 16) & 0xFFFF;

                klib::printf("XHCI:     Found supported protocol USB %u.%u\n", major_revision, minor_revision);
                klib::printf("XHCI:     Port offset: %u, count: %u, flags: %#X\n", port_offset, port_count, port_flags);

                if (major_revision == 2) {
                    for (u8 i = port_offset; i < port_offset + port_count; i++) {
                        ports[i].offset = ports_usb2++;
                        ports[i].type = Port::Type::USB2;
                        ports[i].high_speed_only = port_flags & (1 << 1);
                    }
                } else if (major_revision == 3) {
                    for (u8 i = port_offset; i < port_offset + port_count; i++) {
                        ports[i].offset = ports_usb3++;
                        ports[i].type = Port::Type::USB3;
                    }
                }
            }
            default: return;
            }
        });

        // pair up USB3 ports with USB2 ports
        for (u8 i = 0; i < ports.size(); i++) {
            for (u8 j = 0; j < ports.size(); j++) {
                if (ports[i].offset == ports[j].offset &&
                    ports[i].type == Port::Type::USB3 &&
                    ports[j].type == Port::Type::USB2
                ) {
                    ports[i].pair_index = j;
                    ports[j].pair_index = i;
                    ports[i].has_pair = true;
                    ports[j].has_pair = true;
                }
            }
        }

        for (u8 i = 0; i < ports.size(); i++) {
            if (ports[i].type == Port::Type::UNKNOWN) continue;
            if (!ports[i].has_pair || (ports[i].has_pair && ports[i].type == Port::Type::USB3))
                ports[i].active = true;
        }

        static_assert(PAGE_SIZE == 0x1000);
        if (u32 page_size = op_regs.read<u32>(OpRegs::SUPPORTED_PAGE_SIZE); !(page_size & 1))
            klib::printf("XHCI: Controller does not support the system page size (register: %#X)\n", page_size);

        reset();

        if (sizeof(u64) * (max_device_slots + 1) > PAGE_SIZE) return -ENOMEM;
        dcbaa_page = pmm::alloc_page();
        if (!dcbaa_page) return -ENOMEM;
        dcbaa = dcbaa_page->as<u64>();
        memset(dcbaa, 0, PAGE_SIZE);

        if (max_scratchpad_buffers > 0) {
            if (sizeof(u64) * max_scratchpad_buffers > PAGE_SIZE) return -ENOMEM;
            scratchpad_array_page = pmm::alloc_page();
            if (!scratchpad_array_page) return -ENOMEM;
            scratchpad_array = scratchpad_array_page->as<u64>();
            memset(scratchpad_array, 0, PAGE_SIZE);

            for (usize i = 0; i < max_scratchpad_buffers; i++) {
                auto *scratchpad_page = pmm::alloc_page();
                if (!scratchpad_page) return -ENOMEM;
                memset(scratchpad_page->as<void>(), 0, PAGE_SIZE);
                scratchpad_array[i] = scratchpad_page->phy();
            }

            dcbaa[0] = scratchpad_array_page->phy();
        }

        op_regs.write<u64>(OpRegs::DCBAAP, dcbaa_page->phy());
        mmio::sync();

        command_ring.init();
        op_regs.write<u64>(OpRegs::COMMAND_RING_CONTROL, command_ring.page->phy() | command_ring.cycle_bit);
        mmio::sync();

        auto interrupter_regs = get_interrupter_regs(0);
        interrupter_regs.set_bit<u32>(InterrupterRegs::MANAGEMENT, InterrupterManagement::INTERRUPT_ENABLE);
        mmio::sync();

        event_ring.init(interrupter_regs);

        acknowledge_irq(0);

        pci_device->msix_init();
        if (pci_device->msix.exists) {
            u8 vec = cpu::interrupts::allocate_vector();
            cpu::interrupts::set_isr(vec, [] (void *priv, cpu::InterruptState *) {
                ((Controller*)priv)->irq();
            }, this);
            pci_device->msix_add(0, vec, false, false);
            pci_device->msix_set_mask(false);
        } else {
            klib::printf("XHCI: Controller does not support MSI-X, attempting to use legacy IRQ which might be incorrect\n");
            cpu::interrupts::register_irq(pci_device->interrupt_line, [] (void *priv, cpu::InterruptState *) {
                ((Controller*)priv)->irq();
            }, this);
        }

        free_requests_list.init();
        in_flight_requests_list.init();
        requests_page = pmm::alloc_page();
        if (!requests_page) return -ENOMEM;
        Request *requests = requests_page->as<Request>();
        for (usize i = 0; i < PAGE_SIZE / sizeof(Request); i++) {
            Request *request = new (&requests[i]) Request();
            free_requests_list.add_before(&request->link);
        }

        op_regs.write<u32>(OpRegs::DEVICE_NOTIF_CONTROL, 1 << 1);
        op_regs.write<u32>(OpRegs::CONFIG, max_device_slots);
        mmio::sync();

        if (isize err = start()) return err;
        klib::printf("XHCI: Started controller\n");

        for (u8 i = 0; i < ports.size(); i++)
            if (ports[i].active)
                reset_port(i);

        for (u8 i = 0; i < ports.size(); i++) {
            klib::printf("XHCI: Port %u %s, %s, speed: %u\n",
                i,
                ports[i].active ? "active" : "inactive",
                ports[i].type == Port::Type::USB3 ? "USB 3.0" : "USB 2.0",
                ports[i].speed);
        }

        for (u8 i = 0; i < ports.size(); i++) {
            if (!ports[i].active) continue;
            klib::sync(setup_device(i));
        }

        return 0;
    }

    isize Controller::reset() {
        op_regs.clear_bit<u32>(OpRegs::USB_CMD, UsbCmd::RUN_STOP);
        mmio::sync();

        u32 timeout = 200;
        while (!(op_regs.read<u32>(OpRegs::USB_STATUS) & UsbStatus::HOST_CONTROLLER_HALTED)) {
            if (--timeout == 0) {
                klib::printf("XHCI: Host controller did not halt within 200 ms\n");
                return -ETIMEDOUT;
            }
            sched::timer::hpet::stall_ms(1);
        }

        op_regs.set_bit<u32>(OpRegs::USB_CMD, UsbCmd::HOST_CONTROLLER_RESET);
        mmio::sync();

        timeout = 1000;
        while (
            (op_regs.read<u32>(OpRegs::USB_STATUS) & UsbStatus::CONTROLLER_NOT_READY) || 
            (op_regs.read<u32>(OpRegs::USB_CMD) & UsbCmd::HOST_CONTROLLER_RESET)
        ) {
            if (--timeout == 0) {
                klib::printf("XHCI: Host controller did not reset within 1000 ms\n");
                return -ETIMEDOUT;
            }
            sched::timer::hpet::stall_ms(1);
        }

        sched::timer::hpet::stall_ms(50);

        return 0;
    }

    isize Controller::start() {
        op_regs.set_bit<u32>(OpRegs::USB_CMD, UsbCmd::RUN_STOP | UsbCmd::INTERRUPTER_ENABLE | UsbCmd::HOST_SYSTEM_ERROR_ENABLE);
        mmio::sync();

        u32 timeout = 1000;
        while (op_regs.read<u32>(OpRegs::USB_STATUS) & UsbStatus::HOST_CONTROLLER_HALTED) {
            if (--timeout == 0) {
                klib::printf("XHCI: Host controller did not start within 1000 ms\n");
                return -ETIMEDOUT;
            }
            sched::timer::hpet::stall_ms(1);
        }

        if (op_regs.read<u32>(OpRegs::USB_STATUS) & UsbStatus::CONTROLLER_NOT_READY) {
            klib::printf("XHCI: Host controller not ready\n");
            return -EAGAIN;
        }
        return 0;
    }

    isize Controller::reset_port(u8 port_index) {
        klib::printf("XHCI: Resetting port %u\n", port_index);
        auto &port = ports[port_index];
        auto port_regs = get_port_regs(port_index);

        port_regs.write<u32>(PortRegs::STATUS_CONTROL, PortSC::POWER);
        mmio::sync();

        // make sure port is powered on
        if ((port_regs.read<u32>(PortRegs::STATUS_CONTROL) & PortSC::POWER) == 0) {
            sched::timer::hpet::stall_ms(20);
            if ((port_regs.read<u32>(PortRegs::STATUS_CONTROL) & PortSC::POWER) == 0) {
                klib::printf("XHCI: Failed to power on port %u\n", port_index);
                return -ETIMEDOUT;
            }
        }

        u32 reset_bit = port.type == Port::Type::USB3 ? PortSC::WARM_RESET : PortSC::RESET;
        u32 reset_change_bit = port.type == Port::Type::USB3 ? PortSC::WARM_RESET_CHANGE : PortSC::RESET_CHANGE;

        u32 change_bits = PortSC::CONNECT_STATUS_CHANGE | PortSC::ENABLED_CHANGE | reset_change_bit;

        port_regs.write<u32>(PortRegs::STATUS_CONTROL, PortSC::POWER | change_bits);
        mmio::sync();

        port_regs.write<u32>(PortRegs::STATUS_CONTROL, PortSC::POWER | reset_bit);
        mmio::sync();

        bool successful_reset = false;
        u32 timeout = 500;
        while (!(port_regs.read<u32>(PortRegs::STATUS_CONTROL) & reset_change_bit)) {
            if (--timeout == 0) {
                klib::printf("XHCI: Port %u did not reset within 500 ms\n", port_index);
                break;
            }
            sched::timer::hpet::stall_ms(1);
        }

        klib::printf("Port %u: %#X\n", port_index, port_regs.read<u32>(PortRegs::STATUS_CONTROL));

        if (timeout > 0) {
            sched::timer::hpet::stall_ms(20);
            if (port_regs.read<u32>(PortRegs::STATUS_CONTROL) & PortSC::ENABLED) {
                port_regs.write<u32>(PortRegs::STATUS_CONTROL, PortSC::POWER | change_bits);
                mmio::sync();
                successful_reset = true;
                port.speed = (port_regs.read<u32>(PortRegs::STATUS_CONTROL) >> 10) & 0xF;
            }
        }

        if (!successful_reset) {
            port.active = false;
            if (port.type == Port::Type::USB3) {
                ports[port.pair_index].active = true;
                return reset_port(port.pair_index); // FIXME: a port might get reset twice
            }
        }

        return successful_reset ? 0 : -ENODEV;
    }

    static u16 get_initial_max_packet_size(u8 port_speed) {
        switch (port_speed) {
        case PortSpeed::LOW_SPEED: return 8;
        case PortSpeed::FULL_SPEED:
        case PortSpeed::HIGH_SPEED: return 64;
        default: return 512;
        }
    }

    static void dump_data(u8 *data, u32 length) {
        klib::printf("XHCI: Dumping data:");
        for (u16 i = 0; i < length; i++)
            klib::printf(" %02x", data[i]);
        klib::printf("\n");
    }

    klib::Awaitable<void> Controller::setup_device(u8 port_index) {
        klib::printf("XHCI: Setting up device at port %u\n", port_index);

        klib::printf("XHCI: Sending enable slot command\n");
        TRB result_trb = co_await send_command(TRB { .control = TRB::TYPE_ENABLE_SLOT_CMD << TRB::TYPE_SHIFT_BITS });
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to enable slot for port %u\n", port_index);
            co_return;
        }

        u8 slot_index = result_trb.control >> 24;
        klib::printf("XHCI: Using slot %u for device on port %u\n", slot_index, port_index);

        Device *device = new Device();
        devices[slot_index] = device;
        device->device_context_page = pmm::alloc_page();
        device->input_context_page = pmm::alloc_page();
        device->context_64_byte = context_size_64_bytes;
        device->port_index = port_index;
        device->port_speed = ports[port_index].speed;
        device->slot_id = slot_index;

        memset(device->device_context_page->as<void>(), 0, PAGE_SIZE);
        memset(device->input_context_page->as<void>(), 0, PAGE_SIZE);

        dcbaa[slot_index] = device->device_context_page->phy();
        mmio::sync();

        device->control_ring.init();

        InputControlContext *input_control_context = device->input_context_page->as<InputControlContext>();
        input_control_context->add_flags = (1 << 0) | (1 << 1);
        input_control_context->drop_flags = 0;

        SlotContext *slot_context = (SlotContext*)((uptr)input_control_context + (device->context_64_byte ? 64 : 32));
        slot_context->context_entries_count = 1;
        slot_context->speed = device->port_speed;
        slot_context->root_hub_port_number = device->port_index + 1;

        EndpointContext *control_ep_context = (EndpointContext*)((uptr)slot_context + (device->context_64_byte ? 64 : 32));
        control_ep_context->linear_stream_array = 1;
        control_ep_context->error_count = 3;
        control_ep_context->endpoint_type = EndpointContext::TYPE_CONTROL;
        control_ep_context->max_packet_size = get_initial_max_packet_size(device->port_speed);
        control_ep_context->transfer_ring_dequeue_ptr = device->control_ring.page->phy() | device->control_ring.cycle_bit;
        control_ep_context->average_trb_length = 8;

        // klib::printf("XHCI: Sending address device command with BSR bit\n");
        // result_trb = co_await send_command(TRB {
        //     .parameter = device->input_context_page->phy(),
        //     .control = (TRB::TYPE_ADDRESS_DEVICE_CMD << TRB::TYPE_SHIFT_BITS) | (device->slot_id << 24) | (1 << 9) 
        // });
        // if ((result_trb.status >> 24) != 1) {
        //     klib::printf("XHCI: Failed to address device on slot %u\n", device->slot_id);
        //     co_return;
        // }

        klib::printf("XHCI: Sending address device command\n");
        result_trb = co_await send_command(TRB {
            .parameter = device->input_context_page->phy(),
            .control = (TRB::TYPE_ADDRESS_DEVICE_CMD << TRB::TYPE_SHIFT_BITS) | (device->slot_id << 24)
        });
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to address device on slot %u\n", device->slot_id);
            co_return;
        }

        pmm::Page *data_page = pmm::alloc_page();
        if (!data_page) co_return;
        u8 *data = data_page->as<u8>();
        memset(data, 0xAE, PAGE_SIZE);
        defer { pmm::free_page(data_page); };

        UsbRequestPacket req = {
            .request_type = 0x80, // Device to Host, Standard, Device
            .request = 6, // GET_DESCRIPTOR
            .value = UsbRequestPacket::DescriptorType::DESCRIPTOR_DEVICE << 8,
            .index = 0,
            .length = control_ep_context->max_packet_size
        };

        klib::printf("XHCI: Sending first USB request for device descriptor\n");
        result_trb = co_await send_usb_request(device, req, data_page->phy(), req.length);
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to send first USB request for device on slot %u\n", device->slot_id);
            co_return;
        }
        dump_data(data, req.length);

        u8 device_descriptor_length = data[0];
        u8 max_packet_size = data[7];

        klib::printf("XHCI: Found max packet size: %d\n", max_packet_size);
        if (control_ep_context->max_packet_size != max_packet_size) {
            control_ep_context->max_packet_size = max_packet_size;

            klib::printf("XHCI: Sending evaluate context command\n");
            result_trb = co_await send_command(TRB {
                .parameter = device->input_context_page->phy(),
                .control = (TRB::TYPE_EVALUATE_CONTEXT_CMD << TRB::TYPE_SHIFT_BITS) | (device->slot_id << 24)
            });
            if ((result_trb.status >> 24) != 1) {
                klib::printf("XHCI: Failed to evaluate context for device on slot %u\n", device->slot_id);
                co_return;
            }
        }

        // control_ep_context->max_packet_size = max_packet_size;

        // klib::printf("XHCI: Resetting port again\n");
        // reset_port(port_index);

        // klib::printf("XHCI: Sending actual address device command\n");
        // result_trb = co_await send_command(TRB {
        //     .parameter = device->input_context_page->phy(),
        //     .control = (TRB::TYPE_ADDRESS_DEVICE_CMD << TRB::TYPE_SHIFT_BITS) | (device->slot_id << 24) // | (1 << 9) 
        // });
        // if ((result_trb.status >> 24) != 1) {
        //     klib::printf("XHCI: Failed to address device on slot %u\n", device->slot_id);
        //     co_return;
        // }

        req = {
            .request_type = 0x80, // Device to Host, Standard, Device
            .request = 6, // GET_DESCRIPTOR
            .value = UsbRequestPacket::DescriptorType::DESCRIPTOR_DEVICE << 8,
            .index = 0,
            .length = device_descriptor_length
        };

        klib::printf("XHCI: Sending USB request for full device descriptor\n");
        result_trb = co_await send_usb_request(device, req, data_page->phy(), req.length);
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to send USB request for full device descriptor for device on slot %u\n", device->slot_id);
            co_return;
        }
        dump_data(data, req.length);

        u8 manufacturer_string_index = data[14];
        u8 product_string_index = data[15];
        u8 serial_number_string_index = data[16];

        klib::printf("XHCI: Sending USB request for language string\n");
        result_trb = co_await request_usb_string_descriptor(device, 0, 0, data_page->phy());
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to send USB request for language string for device on slot %u\n", device->slot_id);
            co_return;
        }
        dump_data(data, data[0]);

        u8 num_languages = (data[0] - 2) / 2;
        u16 chosen_lang = 0;
        for (int i = 0; i < num_languages; i++) {
            u16 lang = ((u16*)data)[i + 1];
            if (lang == 0x0409) { // English US
                chosen_lang = lang;
                break;
            }
        }
        if (chosen_lang == 0) chosen_lang = ((u16*)data)[1];

        if (manufacturer_string_index != 0) {
            result_trb = co_await request_usb_string_descriptor(device, manufacturer_string_index, chosen_lang, data_page->phy());
            if ((result_trb.status >> 24) == 1) {
                klib::printf("XHCI: Manufacturer: %.*s\n", data[0] - 2, &data[2]);
            } else {
                klib::printf("XHCI: Failed to request manufacturer string for device on slot %u\n", device->slot_id);
                co_return;
            }
        }
        if (product_string_index != 0) {
            result_trb = co_await request_usb_string_descriptor(device, product_string_index, chosen_lang, data_page->phy());
            if ((result_trb.status >> 24) == 1) {
                klib::printf("XHCI: Product: %.*s\n", data[0] - 2, &data[2]);
            } else {
                klib::printf("XHCI: Failed to request product string for device on slot %u\n", device->slot_id);
                co_return;
            }
        }
        if (serial_number_string_index != 0) {
            result_trb = co_await request_usb_string_descriptor(device, serial_number_string_index, chosen_lang, data_page->phy());
            if ((result_trb.status >> 24) == 1) {
                klib::printf("XHCI: Serial number: %.*s\n", data[0] - 2, &data[2]);
            } else {
                klib::printf("XHCI: Failed to request serial number string for device on slot %u\n", device->slot_id);
                co_return;
            }
        }

        req = {
            .request_type = 0x80, // Device to Host, Standard, Device
            .request = 6, // GET_DESCRIPTOR
            .value = UsbRequestPacket::DescriptorType::DESCRIPTOR_CONFIGURATION << 8 | (0),
            .index = 0,
            .length = 256
        };

        klib::printf("XHCI: Sending USB request for configuration descriptor\n");
        result_trb = co_await send_usb_request(device, req, data_page->phy(), req.length);
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to get USB configuration descriptor for device on slot %u\n", device->slot_id);
            co_return;
        }
        dump_data(data, data[0]);

        u8 config_desc_length = data[0];
        u16 total_config_length = *(u16*)(&data[2]);
        u8 config_value = data[5];
        u8 config_string_index = data[6];

        if (total_config_length > 256) {
            klib::printf("XHCI: USB configuration descriptor is too big (%d bytes)\n", total_config_length);
            co_return;
        }

        klib::Vector<u8> config_desc;
        config_desc.resize(total_config_length);
        memcpy(config_desc.data(), data, total_config_length);

        if (config_string_index != 0) {
            result_trb = co_await request_usb_string_descriptor(device, config_string_index, chosen_lang, data_page->phy());
            if ((result_trb.status >> 24) == 1)
                klib::printf("XHCI: Configuration description: %.*s\n", data[0] - 2, &data[2]);
        }

        input_control_context->add_flags = 0;
        input_control_context->drop_flags = 0;

        u8 current_interface_num = 0;
        u32 ptr = config_desc_length;
        while (ptr < total_config_length) {
            u8 *desc = config_desc.data() + ptr;
            u8 desc_length = desc[0];
            u8 desc_type = desc[1];

            if (desc_length == 0) break;

            dump_data(desc, desc_length);

            switch ((UsbRequestPacket::DescriptorType)desc_type) {
            case UsbRequestPacket::DESCRIPTOR_INTERFACE: {
                u8 interface_num = desc[2];
                u8 class_code = desc[5];
                u8 sub_class = desc[6];
                u8 protocol = desc[7];

                current_interface_num = interface_num;

                if (class_code == 0x3 && protocol == 0x2) {
                    klib::printf("XHCI: Found mouse\n");
                    device->hid_device = new input::usbhid::Mouse();
                    input::main_mouse = (input::usbhid::Mouse*)device->hid_device;
                } else if (class_code == 0x3 && protocol == 0x1) {
                    klib::printf("XHCI: Found keyboard\n");
                    device->hid_device = new input::usbhid::Keyboard();
                    input::main_keyboard = (input::usbhid::Keyboard*)device->hid_device;
                } else {
                    klib::printf("XHCI: Unsupported interface found, skipping (class: %#X, subclass: %#X, protocol: %#X)\n", class_code, sub_class, protocol);
                    ptr += desc_length;
                    while (ptr < total_config_length) {
                        u8 *desc = config_desc.data() + ptr;
                        if (desc[1] == UsbRequestPacket::DESCRIPTOR_INTERFACE || desc[0] == 0)
                            break;
                        ptr += desc[0];
                    }
                }
            } break;
            case UsbRequestPacket::DESCRIPTOR_ENDPOINT: {
                u8 address = desc[2];
                u8 attributes = desc[3];
                u16 max_packet_size = (u16)desc[4] | ((u16)desc[5] << 8);
                u8 specified_interval = desc[6];

                u8 endpoint_type = attributes & 0b11;
                if (endpoint_type != 3) {
                    klib::printf("XHCI: Endpoint type other than interrupt not supported (%d)\n", endpoint_type);
                    break;
                }

                u32 endpoint_interval = 3;
                if (device->port_speed == PortSpeed::LOW_SPEED || device->port_speed == PortSpeed::FULL_SPEED) {
                    for (; endpoint_interval < 10; endpoint_interval++) {
                        u32 next_period_microseconds = 125 * (1 << (endpoint_interval + 1));
                        if (next_period_microseconds > specified_interval * 1000)
                            break;
                    }
                } else {
                    endpoint_interval = specified_interval - 1;
                }

                u8 endpoint_number = address & 0xF;
                bool direction = address >> 7; // OUT is 0, IN is 1
                u32 dci = endpoint_number * 2 + direction;
                if (dci != 3) break;

                auto *endpoint = new Endpoint();
                endpoint->transfer_ring.init();
                endpoint->buffer_page = pmm::alloc_page();
                endpoint->device_context_index = dci;
                endpoint->interrupt_packet_size = max_packet_size;
                device->endpoints[dci - 2] = endpoint;

                EndpointContext *ep_context = (EndpointContext*)((uptr)slot_context + dci * (device->context_64_byte ? 64 : 32));
                memset(ep_context, 0, device->context_64_byte ? 64 : 32);
                ep_context->error_count = 3;
                ep_context->endpoint_type = EndpointContext::EndpointType(endpoint_type | (direction << 2));
                ep_context->transfer_ring_dequeue_ptr = endpoint->transfer_ring.page->phy() | endpoint->transfer_ring.cycle_bit;
                ep_context->max_packet_size = max_packet_size & 0x07FF;
                ep_context->max_burst_size = (max_packet_size & 0x1800) >> 11;
                ep_context->max_esit_payload_low = max_packet_size * (ep_context->max_burst_size + 1);
                ep_context->interval = endpoint_interval;

                input_control_context->add_flags |= 1 << dci;

                slot_context->context_entries_count = klib::max(dci + 1, slot_context->context_entries_count);
            } break;
            case UsbRequestPacket::DESCRIPTOR_HID: {
                u8 num_descriptors = desc[5];
                for (u32 i = 0; i < num_descriptors; i++) {
                    u8 desc_type = desc[6 + i * 3];
                    u16 desc_length = (u16)desc[7 + i * 3] | ((u16)desc[8 + i * 3] << 8);
                    if (desc_type == UsbRequestPacket::DescriptorType::DESCRIPTOR_HID_REPORT) {
                        if (desc_length > PAGE_SIZE) {
                            klib::printf("XHCI: HID report descriptor too big (%u bytes)\n", desc_length);
                            break;
                        }

                        req = {
                            .request_type = 0x81, // Device to Host, Standard, Interface
                            .request = 6, // GET_DESCRIPTOR
                            .value = UsbRequestPacket::DescriptorType::DESCRIPTOR_HID_REPORT << 8 | (0),
                            .index = current_interface_num,
                            .length = desc_length
                        };

                        klib::printf("XHCI: Sending USB request for HID report descriptor\n");
                        result_trb = co_await send_usb_request(device, req, data_page->phy(), req.length);
                        if ((result_trb.status >> 24) != 1) {
                            klib::printf("XHCI: Failed to send USB request for HID report descriptor for device on slot %u\n", device->slot_id);
                            co_return;
                        }
                        dump_data(data, desc_length);

                        if (device->hid_device) {
                            device->hid_device->parse_report_descriptor(data, desc_length);
                        }

                        break;
                    }
                }
            } break;
            default:
                klib::printf("XHCI: Unhandled descriptor type %u in config\n", desc_type);
                break;
            }

            ptr += desc_length;
        }

        if (!device->hid_device) {
            klib::printf("XHCI: No supported HID device found on slot %u\n", device->slot_id);
            co_return;
        }
        if (input_control_context->add_flags == 0) {
            klib::printf("XHCI: No endpoints configured for device on slot %u\n", device->slot_id);
            co_return;
        }
        input_control_context->add_flags |= (1 << 0);

        req = {
            .request_type = 0x00,
            .request = 9, // SET_CONFIGURATION
            .value = config_value
        };

        klib::printf("XHCI: Sending USB set configuration request\n");
        result_trb = co_await send_usb_request_no_data(device, req);
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to send USB set configuration request for device on slot %u\n", device->slot_id);
            co_return;
        }

        klib::printf("XHCI: Sending configure endpoint command\n");
        result_trb = co_await send_command(TRB {
            .parameter = device->input_context_page->phy(),
            .control = (TRB::TYPE_CONFIGURE_ENDPOINT_CMD << TRB::TYPE_SHIFT_BITS) | (device->slot_id << 24)
        });
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to configure endpoints for device on slot %u\n", device->slot_id);
            co_return;
        }

        // req = {
        //     .request_type = 0x21,
        //     .request = 10 // SET_IDLE
        // };

        // klib::printf("XHCI: Sending USB set idle request\n");
        // result_trb = co_await send_usb_request_no_data(device, req);
        // if ((result_trb.status >> 24) != 1) {
        //     klib::printf("XHCI: Failed to send USB set idle request for device on slot %u\n", device->slot_id);
        //     co_return;
        // }

        req = {
            .request_type = 0x21,
            .request = 11, // SET_PROTOCOL
            .value = device->hid_device->report_protocol_usable
        };

        klib::printf("XHCI: Sending USB set protocol request\n");
        result_trb = co_await send_usb_request_no_data(device, req);
        if ((result_trb.status >> 24) != 1) {
            klib::printf("XHCI: Failed to send USB set protocol request for device on slot %u\n", device->slot_id);
            co_return;
        }

        request_usb_interrupt_report(device, device->endpoints[1]);
    }

    pci::Registers Controller::get_interrupter_regs(u32 interrupter) {
        return pci::Registers { .bar = bar, .bar_offset = runtime_regs.bar_offset + RuntimeRegs::INTERRUPTER_SETS + interrupter * 0x20 };
    }

    pci::Registers Controller::get_port_regs(u8 port) {
        return pci::Registers { .bar = bar, .bar_offset = op_regs.bar_offset + OpRegs::PORT_REGISTERS + port * 0x10 };
    }

    void Controller::acknowledge_irq(u32 interrupter) {
        op_regs.write<u32>(OpRegs::USB_STATUS, UsbStatus::EVENT_INTERRUPT);
        mmio::sync();
        get_interrupter_regs(interrupter).set_bit<u32>(InterrupterRegs::MANAGEMENT, InterrupterManagement::INTERRUPT_PENDING);
        mmio::sync();
    }

    void Controller::irq() {
        defer {
            acknowledge_irq(0);
            cpu::interrupts::eoi();
        };

        TRB trb;
        while (event_ring.dequeue(&trb) == 0) {
            u8 type = (trb.control >> TRB::TYPE_SHIFT_BITS) & 0b11'1111;
            // klib::printf("XHCI: Received event type: %u, parameter: %#lX, status: %#X, control: %#X\n", type, trb.parameter, trb.status, trb.control);
            switch (TRB::Type(type)) {
            case TRB::TYPE_TRANSFER_EVENT: {
                u8 slot_id = (trb.control >> 24) & 0xFF;
                u8 endpoint_id = (trb.control >> 16) & 0x1F;
                bool is_event_data = (trb.control >> 2) & 1;
                if (endpoint_id == 1) {
                    if (is_event_data) {
                        Request *request = (Request*)trb.parameter;
                        request->callback.invoke(trb);
                        free_request(request);
                    } else {
                        Request *request;
                        LIST_FOR_EACH(request, &in_flight_requests_list, link) {
                            if (request->trb_phy != trb.parameter)
                                continue;
                            request->callback.invoke(trb);
                            free_request(request);
                            break;
                        }
                    }
                } else {
                    Device *device = devices[slot_id];
                    handle_usb_interrupt(device, device->endpoints[endpoint_id - 2]);
                }
            } break;
            case TRB::TYPE_COMMAND_COMPLETION_EVENT: {
                Request *request;
                LIST_FOR_EACH(request, &in_flight_requests_list, link) {
                    if (request->trb_phy != trb.parameter)
                        continue;
                    request->callback.invoke(trb);
                    free_request(request);
                    break;
                }
            } break;
            case TRB::TYPE_PORT_STATUS_CHANGE_EVENT: {

            } break;
            default: break;
            }
        }
    }

    Request* Controller::alloc_request() {
        if (free_requests_list.is_empty())
            return nullptr;
        Request *request = LIST_HEAD(&free_requests_list, Request, link);
        request->link.remove();
        in_flight_requests_list.add_before(&request->link);
        return request;
    }

    void Controller::free_request(Request *request) {
        request->link.remove();
        free_requests_list.add_before(&request->link);
    }

    klib::RootAwaitable<TRB> Controller::send_command(TRB trb) {
        klib::InterruptLock interrupt_guard;
        Request *request = alloc_request();
        if (!request)
            return TRB {};
        auto awaitable = klib::RootAwaitable<TRB>(&request->callback);

        request->trb_phy = command_ring.enqueue(trb);
        doorbell_regs.write<u32>(0, 0);
        return awaitable;
    }

    klib::RootAwaitable<TRB> Controller::send_usb_request(Device *device, UsbRequestPacket req, uptr output_phy, u32 length) {
        klib::InterruptLock interrupt_guard;
        Request *request = alloc_request();
        if (!request)
            return TRB {};

        TRB setup_stage {
            .parameter = *(u64*)&req,
            .status = 8,
            .control = TRB::TYPE_SETUP_STAGE << TRB::TYPE_SHIFT_BITS | TRB::IMMEDIATE_DATA | TRB::TRANSFER_TYPE_IN_DATA
        };
        TRB data_stage {
            .parameter = output_phy,
            .status = length,
            .control = TRB::TYPE_DATA_STAGE << TRB::TYPE_SHIFT_BITS | TRB::DIRECTION_IN
        };
        TRB status_stage {
            .parameter = 0,
            .status = 0,
            .control = TRB::TYPE_STATUS_STAGE << TRB::TYPE_SHIFT_BITS | TRB::CHAIN
        };
        TRB event_data {
            .parameter = (uptr)request,
            .status = 0,
            .control = TRB::TYPE_EVENT_DATA << TRB::TYPE_SHIFT_BITS | TRB::INTERRUPT_ON_COMPLETION
        };

        auto awaitable = klib::RootAwaitable<TRB>(&request->callback);
        device->control_ring.enqueue(setup_stage);
        device->control_ring.enqueue(data_stage);
        device->control_ring.enqueue(status_stage);
        request->trb_phy = device->control_ring.enqueue(event_data);
        doorbell_regs.write<u32>(device->slot_id * sizeof(u32), 1);
        return awaitable;
    }

    klib::RootAwaitable<TRB> Controller::send_usb_request_no_data(Device *device, UsbRequestPacket req) {
        klib::InterruptLock interrupt_guard;
        Request *request = alloc_request();
        if (!request)
            return TRB {};

        TRB setup_stage {
            .parameter = *(u64*)&req,
            .status = 8,
            .control = TRB::TYPE_SETUP_STAGE << TRB::TYPE_SHIFT_BITS | TRB::IMMEDIATE_DATA
        };
        TRB status_stage {
            .parameter = 0,
            .status = 0,
            .control = TRB::TYPE_STATUS_STAGE << TRB::TYPE_SHIFT_BITS | TRB::DIRECTION_IN | TRB::CHAIN
        };
        TRB event_data {
            .parameter = (uptr)request,
            .status = 0,
            .control = TRB::TYPE_EVENT_DATA << TRB::TYPE_SHIFT_BITS | TRB::INTERRUPT_ON_COMPLETION
        };

        auto awaitable = klib::RootAwaitable<TRB>(&request->callback);
        device->control_ring.enqueue(setup_stage);
        device->control_ring.enqueue(status_stage);
        request->trb_phy = device->control_ring.enqueue(event_data);
        doorbell_regs.write<u32>(device->slot_id * sizeof(u32), 1);
        return awaitable;
    }

    klib::Awaitable<TRB> Controller::request_usb_string_descriptor(Device *device, u8 string_index, u16 lang_id, uptr output_phy) {
        UsbRequestPacket req = {
            .request_type = 0x80, // Device to Host, Standard, Device
            .request = 6, // GET_DESCRIPTOR
            .value = u16(UsbRequestPacket::DescriptorType::DESCRIPTOR_STRING << 8 | (string_index)),
            .index = lang_id,
            .length = 256
        };
        co_return co_await send_usb_request(device, req, output_phy, req.length);
    }

    void Controller::request_usb_interrupt_report(Device *device, Endpoint *endpoint) {
        klib::InterruptLock interrupt_guard;
        TRB trb {
            .parameter = endpoint->buffer_page->phy(),
            .status = endpoint->interrupt_packet_size,
            .control = TRB::TYPE_NORMAL << TRB::TYPE_SHIFT_BITS | TRB::INTERRUPT_ON_COMPLETION
        };
        endpoint->transfer_ring.enqueue(trb);
        doorbell_regs.write<u32>(device->slot_id * sizeof(u32), endpoint->device_context_index);
    }

    void Controller::handle_usb_interrupt(Device *device, Endpoint *endpoint) {
        u8 *data = endpoint->buffer_page->as<u8>();
        device->hid_device->event(data, endpoint->interrupt_packet_size);
        request_usb_interrupt_report(device, endpoint);
    }

    isize EnqueueRing::init() {
        size = PAGE_SIZE / sizeof(TRB);
        enqueue_index = 0;
        cycle_bit = 1;

        page = pmm::alloc_page();
        if (!page) return -ENOMEM;
        ring = page->as<TRB>();
        memset(ring, 0, PAGE_SIZE);

        TRB *link_trb = &ring[size - 1];
        link_trb->parameter = page->phy();
        link_trb->control = (TRB::TYPE_LINK << TRB::TYPE_SHIFT_BITS) | TRB::LINK_TOGGLE_CYCLE | cycle_bit;
        return 0;
    }

    uptr EnqueueRing::enqueue(TRB trb) {
        if (cycle_bit) trb.control |= 1;
        else trb.control &= ~1;

        uptr phy = (uptr)&ring[enqueue_index] - mem::hhdm;
        ring[enqueue_index] = trb;

        enqueue_index++;
        if (enqueue_index == size - 1) {
            TRB *link_trb = &ring[size - 1];
            link_trb->control = (TRB::TYPE_LINK << TRB::TYPE_SHIFT_BITS) | TRB::LINK_TOGGLE_CYCLE | cycle_bit;

            enqueue_index = 0;
            cycle_bit = !cycle_bit;
        }
        return phy;
    }

    isize EventRing::init(pci::Registers interrupter_regs) {
        size = PAGE_SIZE / sizeof(TRB);
        dequeue_index = 0;
        cycle_bit = 1;
        this->interrupter_regs = interrupter_regs;

        segment_table_page = pmm::alloc_page();
        if (!segment_table_page) return -ENOMEM;
        segment_table = segment_table_page->as<SegmentTableEntry>();
        memset(segment_table, 0, PAGE_SIZE);

        ring_page = pmm::alloc_page();
        if (!ring_page) return -ENOMEM;
        ring = ring_page->as<TRB>();
        memset(ring, 0, PAGE_SIZE);

        segment_table[0].address = ring_page->phy();
        segment_table[0].size = size;

        interrupter_regs.write<u32>(InterrupterRegs::EVENT_RING_SEGMENT_TABLE_SIZE, 1);
        mmio::sync();
        interrupter_regs.write<u64>(InterrupterRegs::EVENT_RING_DEQUEUE_POINTER, ring_page->phy() + dequeue_index * sizeof(TRB));
        mmio::sync();
        interrupter_regs.write<u64>(InterrupterRegs::EVENT_RING_SEGMENT_TABLE_BASE_ADDRESS, segment_table_page->phy());
        mmio::sync();

        return 0;
    }

    isize EventRing::dequeue(TRB *trb) {
        if (!has_unprocessed_events())
            return -EWOULDBLOCK;

        *trb = ring[dequeue_index];

        dequeue_index++;
        if (dequeue_index == size) {
            dequeue_index = 0;
            cycle_bit = !cycle_bit;
        }

        interrupter_regs.write<u64>(InterrupterRegs::EVENT_RING_DEQUEUE_POINTER, ring_page->phy() + dequeue_index * sizeof(TRB));
        mmio::sync();
        interrupter_regs.set_bit<u64>(InterrupterRegs::EVENT_RING_DEQUEUE_POINTER, EVENT_HANDLER_BUSY);
        mmio::sync();
        return 0;
    }

    bool EventRing::has_unprocessed_events() {
        return (ring[dequeue_index].control & 1) == cycle_bit;
    }
}
