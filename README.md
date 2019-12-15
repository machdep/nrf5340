# nRF5340

This is a work-in-progress demo for Bluetooth LE on nRF5340.
Nordic libble controller s140 is used in this demo.

Current status: BLE host-layer library is included to the main core app, Nordic link-layer library has set up on the net core, but communication between them has not yet established.

Nordicsemi nRF5340 is a dual-core ARM Cortex-M33.

For nRF5340-DK (application core) connect micro usb cable to J2, for other boards connect UART pins as follows:

| nRF5340           | UART-to-USB adapter  |
| ----------------- | -------------------- |
| P0.22 (TX)        | RX                   |
| P0.20 (RX)        | TX                   |

Network MCU has separate UART:

| nRF5340           | UART-to-USB adapter  |
| ----------------- | -------------------- |
| P0.25 (TX)        | RX                   |
| P0.26 (RX)        | TX                   |

UART baud rate: 115200

Plug Segger JLINK adapter to nRF5340-DK 'Debug in' connector.

This app depends on the [secure bootloader for nRF5340](https://github.com/machdep/nrf-boot).

### Under Linux
    $ sudo apt install gcc-arm-linux-gnueabi
    $ export CROSS_COMPILE=arm-linux-gnueabi-

### Under FreeBSD
    $ sudo pkg install arm-none-eabi-gcc arm-none-eabi-binutils
    $ export CROSS_COMPILE=arm-none-eabi-

### Build
    $ git clone --recursive https://github.com/machdep/nrf5340
    $ cd nrf5340
    $ make app net

## Program the chip using nrfjprog
    $ nrfjprog -f NRF53 --erasepage 0x40000-0x90000
    $ nrfjprog -f NRF53 --program obj/nrf5340-app.hex -r

    $ nrfjprog -f NRF53 --coprocessor CP_NETWORK --erasepage 0x01000000-0x01040000
    $ nrfjprog -f NRF53 --coprocessor CP_NETWORK --program obj/nrf5340-net.hex -r

![alt text](https://raw.githubusercontent.com/machdep/nrf9160/master/images/nrf5340-dk.jpg)
