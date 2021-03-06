# Kugle-Embedded
Embedded firmware code for the STM32H7 board on the Kugle robot running the Sliding mode attitude controller and path following MPC, to a certain extent based on code generated by MATLAB coder

## Important notes
* When creating objects of classes of larger size, use "new" to allocate the memory for the object in heap instead of on the stack
  * Example of how to do this with the object as a pointer: `MPU9250 * imu = new MPU9250(spi);`
  * Example of how to do this while keeping the object as a reference (instead of a pointer) is: `MPU9250& imu = *(new MPU9250(spi));`

## IDE
System Workbench, an IDE based on Eclipse, has been used as the development environment for this embedded firmware. System Workbench is a cross-platform compatible IDE based on Eclipse bundled with both libraries for STM32 devices, GDB debugger and ST-Link debugging capability, enabling easy programming and debugging of embedded applications for STM32 devices.

The IDE can be downloaded from http://www.openstm32.org

## Debugging on Ubuntu
If you get an error similar to _Could not determine GDB version using command_ then you might have installed the 64-bit version of System Workbench. However the bundled GDB debugger is a 32-bit version based on the Linaro releases, why you will have to install the corresponding 32-bit version of _ncurses_.

```bash
sudo apt-get install lib32ncurses5
```

## Firmware update using DFU bootloader
Activative the DFU bootloader through the firmware by calling the `Enter_DFU_Bootloader` function.

Now a new firmware can be loaded into the board using the `dfu-util` tool from http://dfu-util.sourceforge.net/dfuse.html

```bash
./dfu-util -a 0 -s 0x08000000:leave -D KugleFirmware.bin
```

The current firmware (reading the whole flash) can also be retrieved from the board using

```bash
./dfu-util -a 0 -s 0x08000000:2097152 -U read.bin
```

OBS. If you receive a permission error you can either run the command as root (`sudo ...`) or you can install the udev rule.

To reset the board out of DFU the `dfu-tool` built in to Linux can be used

```bash
dfu-tool attach
```
