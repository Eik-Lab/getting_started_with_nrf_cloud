# Getting started with Nordic template

## Author:
Lars Øvergard (lars.overgard@nmbu.no)  
Nordic Semiconductor(Original author for the firmware)

## Purpose

The purpose of this template is to serve as a starting point for people to get started with sending sensor data from their NRF device up to the nordic cloud. 

There are several TODO comments in the code which serves as a guide to get the code started. 
These comments are may not be sufficient to get your spesific sensor up and running, and additional changes might be required. 

The template is based upon Nordic Semiconductors [AGPS](https://github.com/nrfconnect/sdk-nrf/tree/v1.7-branch/samples/nrf9160/agps) example.


## Limitations
This firmware is only tested for:

- nRF Connect SDK v1.9.1
- Modem firmware v1.3.2

## Installation

1) Install the nrf Connect for Desktop: https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk

2) Install the "Toolchain Manager" from the "nrf Connect for Desktop" application

3) Install the "nrf Connect SDK v1.9.1" (this might take a while)

4) Download and install "Visual studio code" https://code.visualstudio.com/download (or other IDE if prefferd)

5) Press "Open VS Code" in the toolchain manager, this will install useful packages for VS code.

<br /><br />


## Build and run

### Using SEGGER for building and flashing (Option 1/2)

#### Segger
When flashing the boards for the first time, it must be done by a debugger, since there is no bootloader on the boards.

1) Press "Open Segger Embedded Studio"

2) File -> Open nRF Connect SDK Project

3) Choose the project folder and board, press Ok

4) To build and flash the unit, press Build -> Build and Run

The device will get flashed and running with the latest project <br />
The build files will be created, even if the debugger cant connect to the device. <br />
If there occurs error, and there has been no changes to the code, <br />
it can help to delete the build folder and try to build again.



### Build files with WEST and upload with nrf programmer (Option 2/2)

#### West
1) Go to the toolchain manager, press arrow -> Open command prompt

2) Go to correct folder
```console
cd path_to_project_folder
```

3) Build the application (add "-p" for a clean build)
```bash
west build -b **INSERT_YOUR_BOARD_NAME**
```

There should now be a build folder in your project folder.

<br /><br />

#### nRF Programmer

1) Open the "NRF Connect for Desktop" application

2) Press Programmer->Open

3) Select the device

4) Drag the the merged.hex or app_update.bin file into the application (location of files in build/zephyr/)

5) Erase all and then write the new files

The device will now be flashed with the newest software.

<br /><br />


## Updating the modem firmware
The modem should have a up to date firmware.
This can be easily get done with these steps.
The modem cannot keep the certificate or connect to nrfcloud without a modem firmware1

1) Go to nordicsemi.no and find the nRF9160 SiP modem firmware (https://www.nordicsemi.com/Products/nRF9160/Download#infotabs)

2) Download the firmware

3) Open the Programmer found in nRF Connect for Deskstop and drag the file into the application

4) Click Write

The modem should now be updated!

<br /><br />


## Creating a certificate
There is one thing needed to connected to nrfcloud.com, a certificate.<br />
Each device must have a unique certificate!

1) Use the at_cert firmware in certificates_fw folder

2) There is a guide in the folder for using the firmware

<br /><br />


## Connecting boards to the cloud
There are two things needed to connect the devices to the nrfCloud, the custom name and a pin.
The board must have a valid certificate before adding the device on nrfcloud!

1) Go to nrfcloud.com

2) Press the green plus (+), upper left.

3) Add a "LTE Device".

4) Press "Skip this step", since we are not setting it up with iBasis sim.

5) Write the custom name and the pin.

6) Add or create the device into a relevant group.

The device is now connected to the cloud and gps-fixes should appear when the device is on.<br />

First time the device connected to the nrfcloud, it will disconnected, even if it is added to an account. <br />
Simply restart the device to make it online.

<br /><br />


## Read logs and see debugging
To see what the device is doing live, you can see the logs and prints live. <br />
This can be useful, since it will show errorcodes, if something is wrong or <br />
just show its current state.

1) Connected to the device with USB

2) Open the "LTE Link Monitor"

3) Choose the device

(Remember to uncheck "Show only supported devices" in the program to see the device.)
