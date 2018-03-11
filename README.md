# YM2151 Arcade Classic

www.aidanlawrence.com

See this project in action here: https://youtu.be/VlT8aX9DRZw

This project is a hardware Video Game Music (VGM) player that uses a genuine YM2151 FM synthesizer chip. 
This project is driven by an STM32 "Blue Pill" board, chosen for it's ample speed, I/O, and best of all, price!

Included in this repository is the source code for the project, all of the schematic files, a Bill of Materials, and a completed 
Ki-CAD printed circuitboard /w Gerber files. Feel free to produce your own board!

# Information about the main sound chips and VGM

The YM2151 was a popular sound synthesizer for classic arcade systems as well as a few higher-end home computers in the late 80's and early 90's.
This project synthesizes music from .VGM files in real time on genuine hardware. There is no emulation here.

VGM stands for Video Game Music, and it is a 44.1KHz logging format that stores real soundchip register information. My player will parse these files and send the data to the appropriate chips. You can learn more about the VGM file format here: http://www.smspower.org/Music/VGMFileFormat

http://www.smspower.org/uploads/Music/vgmspec170.txt?sid=58da937e68300c059412b536d4db2ca0

# SD Card Information
This project is built for full-sized SD cards, but you may use adapters to fit your desired card. You must format your SD card to Fat32 in order for this device to work correctly. Your SD card must only contain uncompressed .vgm files. VGZ FILES WILL NOT WORK! You may download .vgz files and use [7zip](http://www.7-zip.org/download.html) to extract the uncompressed file out of them. Vgm files on the SD card do not need to have the .vgm extension. As long as they contain valid, uncompressed vgm data, they will be read by the program regardless of their name.
You can find VGM files by Googling "myGameName VGM," or by checking out sites like http://vgmrips.net/packs/

# Control Over Serial
You can use a serial connection to control playback features. The commands are as follows:

Command | Result
------------ | -------------
\+ | Next Track
\- | Previous Track
\* | Random Track
\/ | Toggle Shuffle Mode
\. | Toggle Song Looping
r: | Request song

A song request is formatted as follows: ```r:mySongFile.vgm```
Once a song request is sent through the serial console, an attempt will be made to open that song file. The file must exist on the SD card, and spelling/capitalization must be correct.
Need an easy-to-use serial console? [I've made one here.](https://github.com/AidanHockey5/OpenArduinoSerialConsole)

# Schematic
![Schematic](https://github.com/AidanHockey5/YM2151_VGM_STM32/raw/master/Schematic/YM2151_STM32_VGM.sch.png)

[PDF](https://github.com/AidanHockey5/YM2151_VGM_STM32/raw/master/Schematic/YM2151_STM32_VGM.pdf)
