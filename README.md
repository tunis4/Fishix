# Fishix

A hobby kernel for x86_64 with Linux binary compatibility, written from scratch in C++

Notable features:
- Capable of running many Linux programs such as KDE Plasma (X11), GIMP, Minecraft and Factorio
- Runs on real hardware
- USB (xHCI) and PS/2 keyboard/mouse
- Networking (currently UDP-only) with virtio-net

SMP is not yet supported, only one core will be used. Only the Limine bootloader is supported.

The long term goal of the project is for it to be somewhat usable in place of the Linux kernel on a basic desktop system.

![Screenshot](/screenshot.png?raw=true "Screenshot")
*Progress screenshot from 6 March 2026*

## Building and running

The kernel itself uses Meson and you can build it as you would with any other Meson project.
```sh
cd kernel
meson setup --wipe build
meson compile --jobs $(nproc) -C build
```

Alternatively, you can use the Makefile in the project root which contains convenience commands for building the kernel, building an .iso, downloading Limine and running it in QEMU.

Build kernel: `make -j$(nproc)`

Build an .iso and run in QEMU: `SYSROOT=sysroot ISO=/tmp/fishix.iso make -j$(nproc) run`

### Setting up a basic Void Linux sysroot

`XBPS_ARCH=x86_64 xbps-install -S -r sysroot -R "https://repo-default.voidlinux.org/current" base-files bash coreutils runit-void util-linux glibc-locales tzdata which shadow metalog`

Edit `sysroot/etc/default/libc-locales` then run `xbps-reconfigure -r sysroot -f glibc-locales`

The provided distro-files have a bootloader option for launching into a graphical environment with KDE, which expects the sysroot to also have the packages `xorg st kde-plasma kde-baseapps plasma5support` installed. You can modify the distro-files to change this.

## License

This code is licensed under the [MIT License](LICENSE).

See the [NOTICE.md](NOTICE.md) file for attributions.
