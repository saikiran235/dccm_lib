# DCCM
Delta Things Cloud Communication Module
## Overview
- This library will enable ESP32 to reprogram STM32 , AVR MCU's from Delta Things TB cloud.
- This app implements an RPC service which allows to download a given URL and write content to the UART0 or file.


## Pin Connections

DCCM-ESP32     STM32F446 <br>
GPIO 25 -----> PA10 (UART1 RX) <br>
GPIO 26 -----> PA09 (UART1 TX) <br>
GPIO 14 -----> BOOT0 <br>
GPIO 12 -----> RESET <br>
    GND -----> GND   <br>


## Steps to Reprogram STM32 using DCCM
1) Build STM32 application bin file e.g Led_Blink.bin
2) Copy helloworld.bin to Delta Droppy https://dtiot-dev.ddns.net:1443/#/stm32
3) Copy shared url link from droppy for this Led_Blink.bin file 
   { select the link as per your application file Led_Blink.bin is a example}
4) Go to https://dtiot-dev.ddns.net:8080/
5) Goto Dashboard --> ESP32 Control pannel --> select ESP32 device with DCCM logic
6) Click RPC and enter 
   - RPC Command ->  CCM.Fetch
   - RPC Parms ->  {"url": "https://dtiot-dev.ddns.net:1443/$/3IAlmH5Dnp.bin", "file": "Led_Blink.bin"}
   - Click Send RPC Command --> give a minute to file to get upload
   - This will push the Led_Blink.bin file to DCCM ESP32 internal flash memory
7) After successful upload , check if file is uploaded to DCCM by entering FS.List in RPC Method ,{} in Parms
8) Make DCCM(ESP32) STM32 conections as per pin connections discription
9) To reprogram STM32 
   - Click RPC and enter 
   - RPC Command ->  CCM.Cmd
   - RPC Parms ->  {"chip_type": "STM32", "cmd": "Flash","file": "/Led_Blink.bin"}
   - Click Send RPC Command --> give a minute to reprogram STM32.