This program uses:
- ESP-32 Devkit
- 64x32 LED Matrix
- Custom connector PCB, or hand-wire ESP-32 to the matrix.
- 5v, 4a power supply
- Energy monitoring plug running Tasmota. I'm using a flashed TopGreener plug that is now out of production. Any energy monitoring plug running Tasmota should be fine.

Start the ESP code. It will establish an access point named HEATPLUG_MONITOR at 192.168.4.1

Plug in the Tasmota. Easiest to configure it and/or update if it's connected to your 'normal' wifi with internet access. 

Update/configure the tasmota as necessary.

Go to the tasmota console and run the following three commands: 
    Rule1 ON Energy#Current DO WebSend [192.168.4.1] GET /current?value=%value% ENDON 
    Rule1 ON Energy#Current DO WebQuery http://192.168.4.1/current?value=%value% ENDON   // This works better.

    Rule1 1

    TelePeriod 60

Make sure the ESP is up and running. On the Tasmota, change the wifi to connect to HEATPULG_MONITOR AP with password 'powerpass'. The tasmota will reboot and connect to the ESP and start sending current data.

If you need to debug, you'll have to have your debug device connect to the HEATPLUG_MONITOR access point.

If you visit 192.168.4.1/clients you can see the IP addresses of the connected clients. The Tasmota device is likely 192.168.4.2 (or .3 or .4). You can reconfigure and check the Tasmota device there.

Note that it's easy to get confused about which network is which! Just remember the tasmota and the esp are on the 192.168.4.1 network, and if you want to see either of them you'll need to be on the same one.

Trying to fix restart

rule2
ON wifi#connected DO ruletimer1 0 ENDON 
ON wifi#disconnected DO ruletimer1 60 ENDON 
ON rules#timer=1 DO restart 1 ENDON