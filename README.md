# INTEX Saltwater System Model ECO6220G Hat

GPIO Pins

```
GPIO 04  Relais K1 WAT-
GPIO 18  Relais K2 WAT+
GPIO 19  Relais K3 CL-
GPIO 05  Relais K4 CL+
GPIO 23  Relais K5 Main Power for Chlor generation
GPIO 22  Beep  
GPIO 21  Fan
GPIO 15  Low Flow
```

Future:
```
GPIO 34  ADC for Low/High Salt
```

# Configuration and Build

## Static Configuration

Is done during build. The values can be set in `main/config.h`.

## Build

- [https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/](Get Started with esp-idf)
- Build with `idf.py build`
- Flush with `idf.py flush` (GPIO0 to Low)
- Run ESP32: GPIO0 to High

## User Configuration

The ESP32 uses DHCP to connect to your WiFi access point and starts a tiny
web-server on startup. Find with arp-scan your Espressif Inc. ESP32 and
navigate with your browser to port 80.
