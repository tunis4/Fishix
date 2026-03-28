// kernel/src/include/uapi/linux/input.h
// Linux Input Event API untuk Fishix Wayland support

#ifndef _FISHIX_INPUT_H
#define _FISHIX_INPUT_H

#include <klib/common.hpp>

// Event types
#define EV_SYN          0x00
#define EV_KEY          0x01
#define EV_REL          0x02
#define EV_ABS          0x03
#define EV_MSC          0x04
#define EV_SW           0x05
#define EV_LED          0x11
#define EV_SND          0x12
#define EV_REP          0x14
#define EV_FF           0x15
#define EV_PWR          0x16
#define EV_FF_STATUS    0x17
#define EV_MAX          0x1f
#define EV_CNT          (EV_MAX + 1)

// Synchronization events
#define SYN_REPORT      0
#define SYN_CONFIG      1
#define SYN_MT_REPORT   2
#define SYN_DROPPED     3

// Keys and buttons
#define KEY_RESERVED    0
#define KEY_ESC         1
#define KEY_1           2
#define KEY_2           3
#define KEY_3           4
#define KEY_4           5
#define KEY_5           6
#define KEY_6           7
#define KEY_7           8
#define KEY_8           9
#define KEY_9           10
#define KEY_0           11
#define KEY_MINUS       12
#define KEY_EQUAL       13
#define KEY_BACKSPACE   14
#define KEY_TAB         15
#define KEY_Q           16
#define KEY_W           17
#define KEY_E           18
#define KEY_R           19
#define KEY_T           20
#define KEY_Y           21
#define KEY_U           22
#define KEY_I           23
#define KEY_O           24
#define KEY_P           25
#define KEY_LEFTBRACE   26
#define KEY_RIGHTBRACE  27
#define KEY_ENTER       28
#define KEY_LEFTCTRL    29
#define KEY_A           30
#define KEY_S           31
#define KEY_D           32
#define KEY_F           33
#define KEY_G           34
#define KEY_H           35
#define KEY_J           36
#define KEY_K           37
#define KEY_L           38
#define KEY_SEMICOLON   39
#define KEY_APOSTROPHE  40
#define KEY_GRAVE       41
#define KEY_LEFTSHIFT   42
#define KEY_BACKSLASH   43
#define KEY_Z           44
#define KEY_X           45
#define KEY_C           46
#define KEY_V           47
#define KEY_B           48
#define KEY_N           49
#define KEY_M           50
#define KEY_COMMA       51
#define KEY_DOT         52
#define KEY_SLASH       53
#define KEY_RIGHTSHIFT  54
#define KEY_KPASTERISK  55
#define KEY_LEFTALT     56
#define KEY_SPACE       57
#define KEY_CAPSLOCK    58
#define KEY_F1          59
#define KEY_F2          60
#define KEY_F3          61
#define KEY_F4          62
#define KEY_F5          63
#define KEY_F6          64
#define KEY_F7          65
#define KEY_F8          66
#define KEY_F9          67
#define KEY_F10         68
#define KEY_NUMLOCK     69
#define KEY_SCROLLLOCK  70
#define KEY_KP7         71
#define KEY_KP8         72
#define KEY_KP9         73
#define KEY_KPMINUS     74
#define KEY_KP4         75
#define KEY_KP5         76
#define KEY_KP6         77
#define KEY_KPPLUS      78
#define KEY_KP1         79
#define KEY_KP2         80
#define KEY_KP3         81
#define KEY_KP0         82
#define KEY_KPDOT       83
#define KEY_F11         87
#define KEY_F12         88

// Mouse buttons
#define BTN_MOUSE       0x110
#define BTN_LEFT        0x110
#define BTN_RIGHT       0x111
#define BTN_MIDDLE      0x112
#define BTN_SIDE        0x113
#define BTN_EXTRA       0x114
#define BTN_FORWARD     0x115
#define BTN_BACK        0x116
#define BTN_TASK        0x117

// Relative axes
#define REL_X           0x00
#define REL_Y           0x01
#define REL_Z           0x02
#define REL_RX          0x03
#define REL_RY          0x04
#define REL_RZ          0x05
#define REL_HWHEEL      0x06
#define REL_DIAL        0x07
#define REL_WHEEL       0x08
#define REL_MISC        0x09
#define REL_MAX         0x0f
#define REL_CNT         (REL_MAX + 1)

// Absolute axes
#define ABS_X           0x00
#define ABS_Y           0x01
#define ABS_Z           0x02
#define ABS_RX          0x03
#define ABS_RY          0x04
#define ABS_RZ          0x05
#define ABS_THROTTLE    0x06
#define ABS_RUDDER      0x07
#define ABS_WHEEL       0x08
#define ABS_GAS         0x09
#define ABS_BRAKE       0x0a
#define ABS_HAT0X       0x10
#define ABS_HAT0Y       0x11
#define ABS_HAT1X       0x12
#define ABS_HAT1Y       0x13
#define ABS_HAT2X       0x14
#define ABS_HAT2Y       0x15
#define ABS_HAT3X       0x16
#define ABS_HAT3Y       0x17
#define ABS_PRESSURE    0x18
#define ABS_DISTANCE    0x19
#define ABS_TILT_X      0x1a
#define ABS_TILT_Y      0x1b
#define ABS_TOOL_WIDTH  0x1c
#define ABS_VOLUME      0x20
#define ABS_MISC        0x28
#define ABS_MAX         0x3f
#define ABS_CNT         (ABS_MAX + 1)

// Misc events
#define MSC_SERIAL      0x00
#define MSC_PULSELED    0x01
#define MSC_GESTURE     0x02
#define MSC_RAW         0x03
#define MSC_SCAN        0x04
#define MSC_TIMESTAMP   0x05
#define MSC_MAX         0x07
#define MSC_CNT         (MSC_MAX + 1)

// LEDs
#define LED_NUML        0x00
#define LED_CAPSL       0x01
#define LED_SCROLLL     0x02
#define LED_COMPOSE     0x03
#define LED_KANA        0x04
#define LED_MAX         0x0f
#define LED_CNT         (LED_MAX + 1)

// Autorepeat values
#define REP_DELAY       0x00
#define REP_PERIOD      0x01
#define REP_MAX         0x01
#define REP_CNT         (REP_MAX + 1)

// Input ID
struct [[gnu::packed]] input_id {
    u16 bustype;
    u16 vendor;
    u16 product;
    u16 version;
};

// Input ABS info
struct [[gnu::packed]] input_absinfo {
    i32 value;
    i32 minimum;
    i32 maximum;
    i32 fuzz;
    i32 flat;
    i32 resolution;
};

// Input event structure
struct [[gnu::packed]] input_event {
    struct {
        i64 tv_sec;
        i64 tv_usec;
    } time;
    u16 type;
    u16 code;
    i32 value;
};

// IOCTLs
#define EVIOCGVERSION       _IOR('E', 0x01, int)
#define EVIOCGID            _IOR('E', 0x02, struct input_id)
#define EVIOCGREP           _IOR('E', 0x03, unsigned int[2])
#define EVIOCSREP           _IOW('E', 0x03, unsigned int[2])
#define EVIOCGKEYCODE       _IOR('E', 0x04, unsigned int[2])
#define EVIOCGKEYCODE_V2    _IOR('E', 0x04, struct input_keymap_entry)
#define EVIOCSKEYCODE       _IOW('E', 0x04, unsigned int[2])
#define EVIOCSKEYCODE_V2    _IOW('E', 0x04, struct input_keymap_entry)

#define EVIOCGNAME(len)     _IOC(_IOC_READ, 'E', 0x06, len)
#define EVIOCGPHYS(len)     _IOC(_IOC_READ, 'E', 0x07, len)
#define EVIOCGUNIQ(len)     _IOC(_IOC_READ, 'E', 0x08, len)
#define EVIOCGPROP(len)     _IOC(_IOC_READ, 'E', 0x09, len)

#define EVIOCGKEY(len)      _IOC(_IOC_READ, 'E', 0x18, len)
#define EVIOCGLED(len)      _IOC(_IOC_READ, 'E', 0x19, len)
#define EVIOCGSND(len)      _IOC(_IOC_READ, 'E', 0x1a, len)
#define EVIOCGSW(len)       _IOC(_IOC_READ, 'E', 0x1b, len)

#define EVIOCGBIT(ev, len)  _IOC(_IOC_READ, 'E', 0x20 + (ev), len)
#define EVIOCGABS(abs)      _IOR('E', 0x40 + (abs), struct input_absinfo)
#define EVIOCSABS(abs)      _IOW('E', 0xc0 + (abs), struct input_absinfo)

#define EVIOCSFF            _IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect))
#define EVIOCRMFF           _IOW('E', 0x81, int)
#define EVIOCGEFFECTS       _IOR('E', 0x84, int)

#define EVIOCGRAB           _IOW('E', 0x90, int)
#define EVIOCREVOKE         _IOW('E', 0x91, int)
#define EVIOCGMASK          _IOR('E', 0x92, struct input_mask)
#define EVIOCSMASK          _IOW('E', 0x93, struct input_mask)
#define EVIOCCKMASK         _IOW('E', 0x94, int)

// Bus types
#define BUS_PCI         0x01
#define BUS_ISAPNP      0x02
#define BUS_USB         0x03
#define BUS_HIL         0x04
#define BUS_BLUETOOTH   0x05
#define BUS_VIRTUAL     0x06
#define BUS_ISA         0x10
#define BUS_I8042       0x11
#define BUS_XTKBD       0x12
#define BUS_RS232       0x13
#define BUS_GAMEPORT    0x14
#define BUS_PARPORT     0x15
#define BUS_AMIGA       0x16
#define BUS_ADB         0x17
#define BUS_I2C         0x18
#define BUS_HOST        0x19
#define BUS_GSC         0x1A
#define BUS_ATARI       0x1B
#define BUS_SPI         0x1C
#define BUS_RMI         0x1D
#define BUS_CEC         0x1E
#define BUS_INTEL_ISHTP 0x1F

#endif // _FISHIX_INPUT_H
