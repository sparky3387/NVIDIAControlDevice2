NVIDIAControlDevice
Remotely control an NVIDIA shield via USB from HTTP/Websockets

This software runs on an ESP32 S2 making it into a virtual keyboard running a Kodi style websocket server, this allows clients to connect via any of the standard Kodi remote control tools, such as:

- Kassi (Chrome)
- Yatse/Kore (Android)

**Supported Hardware**
-ESP32 S2

**Installation**
This requires the https://github.com/Links2004/arduinoWebSockets websockets library and the inbuilt Arduino_JSON by Arduino library, when connected to power it the ESP32 S2, will power up a AP called Shield-Control-Device with a Wifi password "cCV$OSVd1cKU" to allow you to set this up to your own Wifi Network (i added a screenshot for what this looks like below), might be a good idea to change the password and the port number to something unique (the port number is at the top of the INO file)

![image](https://user-images.githubusercontent.com/1683850/175224074-bb2fbd4e-6a2f-48f4-b4e9-5cd957e08d5e.png)

**Limitations**
Unfortuntely, this does not support authentication for its websocket, as the remote control applications do not currently support this, instead this relies on security through obscurity only allowing connections to the correct path and blocks a client from connecting for five minutes, if it connects to the wrong path 5 times.
I managed to get the time for an action down to 15ms, if you know of any ways to improve this, please submit a patch.

