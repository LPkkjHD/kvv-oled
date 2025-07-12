# KVV-Timeschedule on Adafruit SSD1306
Inspired by the somewhat recent [talk](https://www.youtube.com/watch?v=_qhGuTVMc5A) by Till Harbaum I wanted to get the timetable of the nearest station into my appartement.
As I had a Adafruit SSD1306 OLED display as well as an esp32 sitting around I started modifying the project to work on my hardware.

The fetching and parsing logic of the KVV departures is practically the same as in the snipped provided by Till, however, since the SSD1306s display is much smaller than the eInk one used in the original project we reduce the amount of displayed departures to 3 and scroll the station text.

![PXL_20250712_180901012](https://github.com/user-attachments/assets/f6b508ac-8d86-4834-b0ff-56c968b06a51)

## Usage (aka. I want one as well)
1. clone the repository
2. create a file called `wifi.h` which populates `WIFI_SSID` and `WIFI_PASSWORD` of your network
3. choose your station(s) [\[see this guide\]](#choosing-a-station)
4. connect up the display 
5. flash the firmware

## Choosing a station
I highly recommend watching the [talk](https://www.youtube.com/watch?v=_qhGuTVMc5A) by Till Harbaum in order to understand what's going on.
Basically we need to look up which `STOP_ID` our station corresponds to in the KVVs dictionary of stations.
This can be done by querying the following endpoint with the name of the station.

```
https://www.kvv.de/tunnelEfaDirect.php
    ?action=XSLT_STOPFINDER_REQUEST
    &name_sf=<YOUR_STOPNAME_HERE>
    &outputFormat=JSON
    &type_sf=any
```

In the returned JSON look for your station and take note of the corresponding `stateless` attribute. This is the `STOP_ID`, change the definition of said constant in line `36` in the `kvv-oled.ino`.
