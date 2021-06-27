# esp-idf-can2mqtt
CANbus to mqtt bridge using esp32.   
It's purpose is to be a bridge between a CAN-Bus and a MQTT-Broker.    
You can map CAN-ID to MQTT-Topics and map each payload to a message.   

I inspired from [here](https://github.com/c3re/can2mqtt).

![can2mqtt](https://user-images.githubusercontent.com/6020549/123542717-20c06000-d786-11eb-9938-65af6b57fa94.jpg)

I used this image from [here](http://www.adfweb.com/download/filefold/MN67939_ENG.pdf).

# Software requirement
esp-idf v4.2-dev-2243 or later.   
Use twai(Two-Wire Automotive Interface) driver instead of can driver.   

# Hardware requirements
1. SN65HVD23x CAN-BUS Transceiver   

|SN65HVD23x||ESP32|ESP32-S2||
|:-:|:-:|:-:|:-:|:-:|
|D(CTX)|--|GPIO21|GPIO20|(*1)|
|GND|--|GND|GND||
|Vcc|--|3.3V|3.3V||
|R(CRX)|--|GPIO22|GPIO21|(*1)|
|Vref|--|N/C|N/C||
|CANL|--|||To CAN Bus|
|CANH|--|||To CAN Bus|
|RS|--|GND|GND|(*2)|

(*1) You can change using menuconfig.

(*2) N/C for SN65HVD232

2. Termination resistance   
I used 150 ohms.   


# Test Circuit   
```
   +-----------+ +-----------+ +-----------+ 
   | Atmega328 | | Atmega328 | |   ESP32   | 
   |           | |           | |           | 
   | Transmit  | | Receive   | | 21    22  | 
   +-----------+ +-----------+ +-----------+ 
     |       |    |        |     |       |   
   +-----------+ +-----------+   |       |   
   |           | |           |   |       |   
   |  MCP2515  | |  MCP2515  |   |       |   
   |           | |           |   |       |   
   +-----------+ +-----------+   |       |   
     |      |      |      |      |       |   
   +-----------+ +-----------+ +-----------+ 
   |           | |           | | D       R | 
   |  MCP2551  | |  MCP2551  | |   VP230   | 
   | H      L  | | H      L  | | H       L | 
   +-----------+ +-----------+ +-----------+ 
     |       |     |       |     |       |   
     +--^^^--+     |       |     +--^^^--+
     |   R1  |     |       |     |   R2  |   
 |---+-------|-----+-------|-----+-------|---| BackBorn H
             |             |             |
             |             |             |
             |             |             |
 |-----------+-------------+-------------+---| BackBorn L

      +--^^^--+:Terminaror register
      R1:120 ohms
      R2:150 ohms(Not working at 120 ohms)
```

# Installation for ESP32
```
git clone https://github.com/nopnop2002/esp-idf-can2mqtt
cd esp-idf-can2mqtt
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

# Installation for ESP32-S2
```
git clone https://github.com/nopnop2002/esp-idf-can2mqtt
cd esp-idf-can2mqtt
idf.py set-target esp32s2
idf.py menuconfig
idf.py flash
```

# Configuration
![config-main](https://user-images.githubusercontent.com/6020549/123541714-dbe5fa80-d780-11eb-85da-648c201b9a9c.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/123541716-df798180-d780-11eb-82d4-78b82b8fb3b1.jpg)

## CAN Setting
![config-can](https://user-images.githubusercontent.com/6020549/123541727-ebfdda00-d780-11eb-9c83-3f01db84e339.jpg)

## WiFi Setting
![config-wifi](https://user-images.githubusercontent.com/6020549/123541729-f4eeab80-d780-11eb-90b9-f9583764acb8.jpg)

## MQTT Server Setting
![config-mqtt](https://user-images.githubusercontent.com/6020549/123541731-fcae5000-d780-11eb-810a-986e23d2f623.jpg)

# Definition from CANbus to MQTT
When CANbus data is received, it is sent by MQTT according to csv/can2mqtt.csv.   
The file can2mqtt.csv has three columns.   
In the first column you need to specify the CAN Frame type.   
The CAN frame type is either S(Standard frame) or E(Extended frame).   
In the second column you have to specify the CAN-ID as a __hexdecimal number__.    
In the last column you have to specify the MQTT-Topic.   
Each CAN-ID and each MQTT-Topic is allowed to appear only once in the whole file.   

```
S,101,/can/std/101
E,101,/can/ext/101
S,103,/can/std/103
E,103,/can/ext/103
```

When a Standard CAN frame with ID 0x101 is received, it is sent by TOPIC of "/can/std/101".   
When a Extended CAN frame with ID 0x101 is received, it is sent by TOPIC of "/can/ext/101".   


# Definition from MQTT to CANbus
When MQTT data is received, it is sent by CANbus according to csv/mqtt2can.csv.   
Same format as can2mqtt.csv.   
```
S,201,/can/std/201
E,201,/can/ext/201
S,203,/can/std/203
E,203,/can/ext/203
```

When receiving the TOPIC of "/can/std/201", send the Standard CAN frame with ID 0x201.   
When receiving the TOPIC of "/can/ext/201", send the Extended CAN frame with ID 0x201.   


# Recive MQTT data using mosquitto_sub
```mosquitto_sub -h 192.168.10.40 -p 1883 -t '/can/#' -F %X -d```

![can2mqtt-1](https://user-images.githubusercontent.com/6020549/123541739-0637b800-d781-11eb-9e4d-1645cfdd28f1.jpg)

# Transmit CANbus data using mosquitto_pub
- Send standard frame data with CANID = 0x201.   
```echo -ne "\x01\x02\x03" | mosquitto_pub -h 192.168.10.40 -p 1883 -t '/can/std/201' -s```

![can2mqtt-11](https://user-images.githubusercontent.com/6020549/123541747-12237a00-d781-11eb-8132-72687fd75642.jpg)
![can2mqtt-12](https://user-images.githubusercontent.com/6020549/123541749-13ed3d80-d781-11eb-8f4a-2851a938da9b.jpg)

- Send extended frame data with CANID = 0x201.   
```echo -ne "\x11\x12\x13" | mosquitto_pub -h 192.168.10.40 -p 1883 -t '/can/ext/201' -s```

![can2mqtt-13](https://user-images.githubusercontent.com/6020549/123541775-32ebcf80-d781-11eb-8429-f066f2353739.jpg)
![can2mqtt-14](https://user-images.githubusercontent.com/6020549/123542301-ea81e100-d783-11eb-94ae-2cb3ba67d24c.jpg)

# Recive MQTT data using python
```
python -m pip install -U paho-mqtt
python pub.py
```

