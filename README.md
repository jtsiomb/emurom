emurom - ROM emulator board
===========================

![emurom photo](http://nuclear.mutantstargoat.com/hw/emurom/emurom_test_sm.jpg)

The purpose of emurom is to make it fast and convenient to hack code in ROMs
without having to repeatedly remove and reprogram an actual ROM chip.

Emurom is a board which connects through USB to a host computer, and
can be connected in place of a parallel ROM chip on a target device. It acts as
a ROM chip, but holds its data in a static RAM, which can be updated at any time
through the USB connection. Optionally it can provide power (5v) to the device
by shorting a jumper, and it also has a lead to clip to the reset signal of the
target device, to automatically reboot it when a new ROM image is uploaded.

Hardware design (kicad project) is in the root directory. The firmware for the
microcontroller is located under `fw`, while in the `emurom` subdirectory, there
is a UNIX program which can be used to upload a ROM image to the device, simply
by running it with the name of the ROM image as a command line argument.

 - Project website: http://nuclear.mutantstargoat.com/hw/emurom
 - Git repository: https://github.com/jtsiomb/emurom
 - Schematic (PDF): http://nuclear.mutantstargoat.com/hw/emurom/emurom-schematic.pdf

License
-------
Copyright (C) 2020 John Tsiombikas <nuclear@member.fsf.org>

All hardware designs in this project are free/open hardware. Feel free to
manufacture, sell, use, modify and/or redistribute, under the terms of the
Creative Commons Attribution Share-Alike license (CC BY-SA). See `LICENSE.hw`
for details.

All software in this project (firmware and host upload program) are free
software. Feel free to use, modify and/or redistribute under the terms of the
GNU General Public License version 3, or at your option any later version
published by the Free Software foundation. See `LICENSE.sw` for details.
