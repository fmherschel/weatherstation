# Overview

## Devices connected via SCL/SDA
* BMP180
* RTC
* LCD/I2C

## Devices connected via designated data pin
* DHT22 (Pin D2)
* Rotary (Pins D3, D4, D5)

## Devices using 5V
* DHT22
* Rotary
* RTC
* LCD/IC2

## Devices using 3.3V
* BMP180

# Summary - Arduino connections needed

## power supply 
* 5V
* 3.3V
* GND

## Datapins
* 4 Pins D2-D5

## SCL/SDA Pins
* 2 Pins A4+A5

# Device connections

## Arduino Nano

* D5 -> Rotary Pin1
* D4 -> Rotary Pin2
* D3 -> Rotary Pin3
* D2 -> DHT22 Data (Pin2)

* 3.3V -> BM180 VIN
* A4 -> [SDA] 
* A5 -> [SCL] 
* 5V -> [5V]
* GND-> [GND]

## DHT22

* VIN -> [5V]
* Data -> D2
* GND -> [GND]

## BMP180

* SDA -> [SDA]
* SCL -> [SCL]
* GND -> [GND]
* VIN -> 3.3V

## Rotary encoder

* Pin1 -> D5
* Pin2 -> D4
* Pin3 -> D3
* VIN -> [5V]
* GND -> [GND]

## RTC

* SCL -> [SCL]
* SDA -> [SDA]
* VIN -> [5V]
* GND -> [GND]

## LCD 2004/I2C

* SCL -> [SCL]
* SDA -> [SDA]
* VCC -> [5V]
* GND -> [GND]




