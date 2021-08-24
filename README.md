# INTEX Saltwater System Model ECO6220G Hat

GPIO Pins

```
(A) GPIO 23  Relais K5 Main Power for Chlor generation
(B) GPIO 04  Relais K1 WAT-
(C) GPIO 18  Relais K2 WAT+
(D) GPIO 19  Relais K3 CL-
(E) GPIO 05  Relais K4 CL+
(F) GPIO 21  Fan
(G) GPIO 22  Beep  
(H) GPIO 15  Low Flow
```

Future:
```
(?) GPIO 34  ADC for Low/High Salt
```

# Connect ESP32

TBD

![ECO6220G main board](https://github.com/cspiel1/pool/blob/main/img.jpeg "ECO6220G main board")

# Configuration and Build

## Static Configuration

Is done during build. The values can be set in `main/config.h`. Use
`main/config.h.def` as template!

## Build

- [Get Started with esp-idf](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started)
- In your esp-idf directory invoke `. ./export.sh`
- Build with `idf.py build`
- Flush with `idf.py flush` (GPIO0 to Low)
- Run ESP32: GPIO0 to High

## User Configuration

The ESP32 uses DHCP to connect to your WiFi access point and starts a tiny
web-server on startup. Find with arp-scan your Espressif Inc. ESP32 and
navigate with your browser to port 80.
