# smart_balkon
A rfduino/raspberrypi project to water plants on the balkony

That's the plan:

## rfduino part
The rfduino is on the balkony, driven by a battery, records sensor data (weather, moisture, water flow) and waters the plants according to rules (interval, duration, moisture sensor ...)
Sensor data is recorded and can be retrieved via BLE for certain time (eg past 24 h) until its overwritten.

## raspberrypi part
The raspberrypi collects sensor data in regular intervals and can be used to update the rules.
Sensordata are depicted as charts.
(in principle this part could be done also by a smartphone)
