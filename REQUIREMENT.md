# Requirements

## Mesh Network
- 3 ESP32 are in the same mesh network.
- all the 3 nodes act same. they can relay the messages if one-to-one direct message is not available due to distance or other reasons.

## Unified Message Protocol


## Shared Sensors Data
- STM32-A has XOSS Speed Sensor
- STM32-B have DHT11 (temperature data and humidity) Sensor.
- STM32-C have MPU6050(Gyro) Sensor.


- I want to 3 ESP32 to keep sharing data. so every STM32 have a unified dashboard.
- A (Head) sends speed data to rest.
- B (Middle) sends temperature and humidity data to the rest.
- C (Tail) sends gyro sensor to the rest.

## Button Communication
- Broadcasting messages via buttons
- Button 1: Pace Up -> green light -> play pace_up.mp3
- Button 2: Pace Down -> yellow light -> play pace_down.mp3
- Button 3: STOP! -> red light -> play stop.mp3
