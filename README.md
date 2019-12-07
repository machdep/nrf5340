# nRF5340

Nordicsemi nRF5340 is a dual-core ARM Cortex-M33.

For nRF5340-DK connect micro usb cable to J2, for other boards connect UART pins as follows:

| nRF5340          | UART-to-USB adapter  |
| ----------------- | -------------------- |
| P0.22 (TX)        | RX                   |
| P0.20 (RX)        | TX                   |

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
    $ make

## Program the chip using nrfjprog
    $ nrfjprog -f NRF53 --erasepage 0x40000-0x90000
    $ nrfjprog -f NRF53 --program obj/nrf5340.hex -r

## Program the chip using OpenOCD

### Build openocd
    $ sudo apt install pkg-config autotools-dev automake libtool
    $ git clone https://github.com/bukinr/openocd-nrf9160
    $ cd openocd-nrf9160
    $ ./bootstrap
    $ ./configure --enable-jlink
    $ make

### Invoke openocd
    $ export OPENOCD_PATH=/path/to/openocd-nrf9160
    $ sudo ${OPENOCD_PATH}/src/openocd -s ${OPENOCD_PATH}/tcl \
      -f interface/jlink.cfg -c 'transport select swd; adapter_khz 1000' \
      -f target/nrf9160.cfg -c "program nrf9160.elf 0 reset verify exit"

![alt text](https://raw.githubusercontent.com/machdep/nrf9160/master/images/nrf5340-dk.jpg)
