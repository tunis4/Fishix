#pragma once

#include <dev/pci.hpp>
#include <mem/pmm.hpp>
#include <klib/vector.hpp>
#include <klib/async.hpp>
#include <dev/input/usbhid.hpp>

namespace dev::xhci {
    struct [[gnu::packed]] TRB {
        u64 parameter;
        u32 status;
        u32 control;

        enum Type : u32 {
            // Transfer Ring
            TYPE_NORMAL = 1,
            TYPE_SETUP_STAGE = 2,
            TYPE_DATA_STAGE = 3,
            TYPE_STATUS_STAGE = 4,
            TYPE_ISOCH = 5,
            TYPE_LINK = 6,
            TYPE_EVENT_DATA = 7,
            TYPE_NO_OP = 8,

            // Command Ring
            TYPE_ENABLE_SLOT_CMD = 9,
            TYPE_DISABLE_SLOT_CMD = 10,
            TYPE_ADDRESS_DEVICE_CMD = 11,
            TYPE_CONFIGURE_ENDPOINT_CMD = 12,
            TYPE_EVALUATE_CONTEXT_CMD = 13,
            TYPE_RESET_ENDPOINT_CMD = 14,
            TYPE_STOP_ENDPOINT_CMD = 15,
            TYPE_SET_TR_DEQUEUE_POINTER_CMD = 16,
            TYPE_RESET_DEVICE_CMD = 17,
            TYPE_FORCE_HEADER_CMD = 22,
            TYPE_NO_OP_CMD = 23,
            TYPE_GET_EXTENDED_PROPERTY_CMD = 24,
            TYPE_SET_EXTENDED_PROPERTY_CMD = 25,

            // Event Ring
            TYPE_TRANSFER_EVENT = 32,
            TYPE_COMMAND_COMPLETION_EVENT = 33,
            TYPE_PORT_STATUS_CHANGE_EVENT = 34,
            TYPE_HOST_CONTROLLER_EVENT = 37,
            TYPE_DEVICE_NOTIFICATION_EVENT = 38,
            TYPE_MFINDEX_WRAP_EVENT = 39,
        };

        static constexpr u32 TYPE_SHIFT_BITS = 10;

        static constexpr u32 LINK_TOGGLE_CYCLE = 1 << 1;
        static constexpr u32 INTERRUPT_ON_COMPLETION = 1 << 5;
        static constexpr u32 IMMEDIATE_DATA = 1 << 6;

        // setup stage
        static constexpr u32 TRANSFER_TYPE_OUT_DATA = 2 << 16;
        static constexpr u32 TRANSFER_TYPE_IN_DATA = 3 << 16;

        // data stage
        static constexpr u32 EVALUATE_NEXT = 1 << 1;
        static constexpr u32 CHAIN = 1 << 4;
        static constexpr u32 DIRECTION_IN = 1 << 16;
    };

    struct [[gnu::packed]] InputControlContext {
        u32 drop_flags;
        u32 add_flags;
    };

    struct [[gnu::packed]] SlotContext {
        enum SlotState : u8 {
            SLOT_STATE_DISABLED_OR_ENABLED = 0,
            SLOT_STATE_DEFAULT = 1,
            SLOT_STATE_ADDRESSED = 2,
            SLOT_STATE_CONFIGURED = 3
        };

        struct {
            u32 route_string : 20;
            u8 speed : 4;
            bool _reserved0 : 1;
            bool multi_tt : 1;
            bool hub : 1;
            u8 context_entries_count : 5;
        };
        u16 max_exit_latency;
        u8 root_hub_port_number; // 1-based
        u8 num_ports; // 0 if device is not a hub
        u32 zero;
        struct {
            u8 device_address : 8;
            u32 _reserved1 : 19;
            SlotState slot_state : 5;
        };
    };

    struct [[gnu::packed]] EndpointContext {
        enum EndpointType : u8 {
            TYPE_INVALID = 0,
            TYPE_ISOCHRONOUS_OUT = 1,
            TYPE_BULK_OUT = 2,
            TYPE_INTERRUPT_OUT = 3,
            TYPE_CONTROL = 4,
            TYPE_ISOCHRONOUS_IN = 5,
            TYPE_BULK_IN = 6,
            TYPE_INTERRUPT_IN = 7,
        };

        struct {
            u8 endpoint_state : 3;
            u8 _reserved0 : 5;
            u8 mult : 2;
            u8 max_primary_streams : 5;
            bool linear_stream_array : 1;
            u8 interval : 8;
            u8 max_esit_payload_high : 8;
        };
        struct {
            u8 _reserved1 : 1;
            u8 error_count : 2;
            EndpointType endpoint_type : 3;
            u8 _reserved2 : 1;
            bool host_initiate_disable : 1;
            u8 max_burst_size : 8;
            u16 max_packet_size : 16;
        };
        u64 transfer_ring_dequeue_ptr; // bit 1 is cycle state
        u16 average_trb_length;
        u16 max_esit_payload_low;
    };

    struct [[gnu::packed]] UsbRequestPacket {
        enum DescriptorType {
            DESCRIPTOR_DEVICE = 0x01,
            DESCRIPTOR_CONFIGURATION = 0x02,
            DESCRIPTOR_STRING = 0x03,
            DESCRIPTOR_INTERFACE = 0x04,
            DESCRIPTOR_ENDPOINT = 0x05,
            DESCRIPTOR_DEVICE_QUALIFIER = 0x06,
            DESCRIPTOR_OTHER_SPEED_CONFIGURATION = 0x07,
            DESCRIPTOR_INTERFACE_POWER = 0x08,
            DESCRIPTOR_OTG = 0x09,
            DESCRIPTOR_DEBUG = 0x0A,
            DESCRIPTOR_INTERFACE_ASSOCIATION = 0x0B,
            DESCRIPTOR_BOS = 0x0F,
            DESCRIPTOR_DEVICE_CAPABILITY = 0x10,
            DESCRIPTOR_WIRELESS_ENDPOINT_COMPANION = 0x11,
            DESCRIPTOR_SUPERSPEED_ENDPOINT_COMPANION = 0x30,
            DESCRIPTOR_SUPERSPEEDPLUS_ISO_ENDPOINT_COMPANION = 0x31,

            DESCRIPTOR_HID = 0x21,
            DESCRIPTOR_HID_REPORT = 0x22,
            DESCRIPTOR_HID_PHYSICAL_REPORT = 0x23,

            DESCRIPTOR_HUB = 0x29,
            DESCRIPTOR_SUPERSPEED_HUB = 0x2A,

            DESCRIPTOR_BILLBOARD = 0x0D,

            DESCRIPTOR_TYPE_C_BRIDGE = 0x0E,
        };

        union {
            struct {
                u8 recipient : 5;
                u8 type : 2;
                u8 transfer_direction : 1;
            };
            u8 request_type;
        };
        u8 request;
        u16 value;
        u16 index;
        u16 length;
    };

    struct Request {
        klib::ListHead link;
        uptr trb_phy;
        klib::RequestCallback<TRB> callback;
    };

    struct EnqueueRing {
        pmm::Page *page;
        TRB *ring;
        usize size;
        usize enqueue_index;
        bool cycle_bit;

        isize init();
        // returns physical address of enqueued trb
        uptr enqueue(TRB trb);
    };

    struct EventRing {
        struct [[gnu::packed]] SegmentTableEntry {
            u64 address;
            u64 size;
        };

        static constexpr u64 EVENT_HANDLER_BUSY = 1 << 3;

        pmm::Page *segment_table_page, *ring_page;
        TRB *ring;
        SegmentTableEntry *segment_table;
        usize size;
        usize dequeue_index;
        bool cycle_bit;
        pci::Registers interrupter_regs;

        isize init(pci::Registers interrupter_regs);
        isize dequeue(TRB *trb);
        bool has_unprocessed_events();
    };

    struct Port {
        enum class Type : u8 {
            UNKNOWN = 0,
            USB2, USB3
        };

        u8 index; // 0-based index of the port itself
        u8 offset; // offset of the port within its protocol
        u8 pair_index;
        Type type;
        u8 speed;
        bool high_speed_only; // for usb2
        bool has_pair;
        bool active;
    };

    struct Endpoint {
        EnqueueRing transfer_ring;
        pmm::Page *buffer_page;
        u8 device_context_index;
        u8 interrupt_packet_size;
    };

    struct Device {
        u8 port_index; // 0-based
        u8 slot_id; // 1-based slot index in the DCBAA
        u8 port_speed;
        bool context_64_byte;

        pmm::Page *device_context_page;
        pmm::Page *input_context_page;

        EnqueueRing control_ring;
        Endpoint *endpoints[30]; // indexed with dci - 2

        input::usbhid::Device *hid_device = nullptr; // hacky and temporary solution
    };

    struct Controller {
        pci::Device *pci_device;
        pci::BAR *bar;
        pci::Registers cap_regs;
        pci::Registers op_regs;
        pci::Registers runtime_regs;
        pci::Registers doorbell_regs;

        pmm::Page *dcbaa_page, *scratchpad_array_page;
        u64 *dcbaa, *scratchpad_array;

        EnqueueRing command_ring;
        EventRing event_ring;

        klib::Vector<Device*> devices; // indexed by slot index
        klib::Vector<Port> ports;

        pmm::Page *requests_page;
        klib::ListHead free_requests_list, in_flight_requests_list;

        isize init();
        isize reset();
        isize start();
        isize reset_port(u8 port_index);

        klib::Awaitable<void> setup_device(u8 port_index);

        Request* alloc_request();
        void free_request(Request *request);

        klib::RootAwaitable<TRB> send_command(TRB trb);
        klib::RootAwaitable<TRB> send_usb_request(Device *device, UsbRequestPacket req, uptr output_phy, u32 length);
        klib::RootAwaitable<TRB> send_usb_request_no_data(Device *device, UsbRequestPacket req);
        klib::Awaitable<TRB> request_usb_string_descriptor(Device *device, u8 string_index, u16 lang_id, uptr output_phy);
        void request_usb_interrupt_report(Device *device, Endpoint *endpoint);
        void handle_usb_interrupt(Device *device, Endpoint *endpoint);

        pci::Registers get_interrupter_regs(u32 interrupter);
        pci::Registers get_port_regs(u8 port);
        void acknowledge_irq(u32 interrupter);

        void irq();

        template<typename F>
        void iterate_extended_capabilities(F func) {
            u32 cap_ptr = extended_capabilities_offset;
            while (true) {
                u32 cap_first = cap_regs.read<u32>(cap_ptr);
                u8 cap_id = cap_first & 0xFF;
                func(cap_ptr, cap_first, cap_id);
                u32 next_cap_offset = sizeof(u32) * ((cap_first >> 8) & 0xFF);
                if (next_cap_offset == 0) break;
                cap_ptr += next_cap_offset;
            }
        }

    private:
        // HCSPARAMS1
        u8  max_device_slots;
        u16 max_interrupters;
        u8  max_ports;

        // HCSPARAMS2
        u8 max_scratchpad_buffers;

        // HCCPARAMS1
        bool context_size_64_bytes;
        u32 extended_capabilities_offset;
    };

    namespace OpRegs {
        constexpr u32 USB_CMD = 0;
        constexpr u32 USB_STATUS = 4;
        constexpr u32 SUPPORTED_PAGE_SIZE = 8;
        constexpr u32 DEVICE_NOTIF_CONTROL = 20;
        constexpr u32 COMMAND_RING_CONTROL = 24;
        constexpr u32 DCBAAP = 48;
        constexpr u32 CONFIG = 56;
        constexpr u32 PORT_REGISTERS = 0x400;
    }

    namespace PortRegs {
        constexpr u32 STATUS_CONTROL = 0x0;
        constexpr u32 POWER_MANAGEMENT_STATUS_CONTROL = 0x4;
        constexpr u32 LINK_INFO = 0x8;
        constexpr u32 HARDWARE_LPM_CONTROL = 0xC;
    }

    namespace PortSC {
        constexpr u32 CONNECT_STATUS = 1 << 0;
        constexpr u32 ENABLED = 1 << 1;
        constexpr u32 RESET = 1 << 4;
        constexpr u32 POWER = 1 << 9;
        constexpr u32 CONNECT_STATUS_CHANGE = 1 << 17;
        constexpr u32 ENABLED_CHANGE = 1 << 18;
        constexpr u32 WARM_RESET_CHANGE = 1 << 19;
        constexpr u32 OVER_CURRENT_CHANGE = 1 << 20;
        constexpr u32 RESET_CHANGE = 1 << 21;
        constexpr u32 WARM_RESET = 1 << 31;
    }

    namespace PortSpeed {
        constexpr u32 FULL_SPEED = 1;
        constexpr u32 LOW_SPEED = 2;
        constexpr u32 HIGH_SPEED = 3;
        constexpr u32 SUPER_SPEED = 4;
    };

    namespace RuntimeRegs {
        constexpr u32 MICROFRAME_INDEX = 0;
        constexpr u32 INTERRUPTER_SETS = 32;
    }

    namespace InterrupterRegs {
        constexpr u32 MANAGEMENT = 0;
        constexpr u32 MODERATION = 4;
        constexpr u32 EVENT_RING_SEGMENT_TABLE_SIZE = 8;
        constexpr u32 EVENT_RING_SEGMENT_TABLE_BASE_ADDRESS = 16;
        constexpr u32 EVENT_RING_DEQUEUE_POINTER = 24;
    }

    namespace InterrupterManagement {
        constexpr u32 INTERRUPT_PENDING = 1 << 0;
        constexpr u32 INTERRUPT_ENABLE = 1 << 1;
    }

    namespace UsbCmd {
        constexpr u32 RUN_STOP = 1 << 0;
        constexpr u32 HOST_CONTROLLER_RESET = 1 << 1;
        constexpr u32 INTERRUPTER_ENABLE = 1 << 2;
        constexpr u32 HOST_SYSTEM_ERROR_ENABLE = 1 << 3;
    }

    namespace UsbStatus {
        constexpr u32 HOST_CONTROLLER_HALTED = 1 << 0;
        constexpr u32 HOST_SYSTEM_ERROR = 1 << 2;
        constexpr u32 EVENT_INTERRUPT = 1 << 3;
        constexpr u32 PORT_CHANGE_DETECT = 1 << 4;
        constexpr u32 SAVE_STATE_STATUS = 1 << 8;
        constexpr u32 RESTORE_STATE_STATUS = 1 << 9;
        constexpr u32 SAVE_RESTORE_ERROR = 1 << 10;
        constexpr u32 CONTROLLER_NOT_READY = 1 << 11;
        constexpr u32 HOST_CONTROLLER_ERROR = 1 << 12;
    }
}
