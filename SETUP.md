# ESP8266 RC Car - Platform IO Setup Guide

## Quick Start Steps

### 1. Install Platform IO
- Install VS Code if not already installed
- Go to VS Code Extensions (Ctrl+Shift+X)
- Search for "PlatformIO IDE"
- Click Install

### 2. Update Configuration
Before building, configure these files:

**platformio.ini:**
- Change `monitor_port = COM3` to your ESP8266's COM port

**src/main.cpp:**
- Update WiFi credentials (lines ~8-9):
  ```cpp
  const char* ssid = "YOUR_SSID";
  const char* password = "YOUR_PASSWORD";
  ```

### 3. Build & Upload
In VS Code with the project open:
1. Click the **PlatformIO Home** icon (house icon in status bar)
2. Or use the Command Palette (Ctrl+Shift+P) → "PlatformIO: Build"
3. Once built, use "PlatformIO: Upload" to flash to your ESP8266
4. Use "PlatformIO: Monitor" to view serial output (115200 baud)

### 4. Access the Web Interface
1. Check serial monitor for ESP8266's IP address
2. Open browser and navigate to: `http://<IP_ADDRESS>`
3. Use the control panel to drive your RC car

## Hardware Connections Summary

### Motor Driver (L293D):
```
ESP8266 D0 (GPIO16)  → L293D Pin 1A  (Left motor IN1)
ESP8266 D1 (GPIO5)   → L293D Pin 2A  (Left motor IN2)
ESP8266 D2 (GPIO4)   → L293D Pin 3A  (Right motor IN1)
ESP8266 D3 (GPIO0)   → L293D Pin 4A  (Right motor IN2)
ESP8266 D4 (GPIO2)   → L293D Pin 1EN (Left motor PWM)
ESP8266 D5 (GPIO14)  → L293D Pin 2EN (Right motor PWM)

L293D Pin 1OUT → Left Motor (+)
L293D Pin 2OUT → Left Motor (-)
L293D Pin 3OUT → Right Motor (+)
L293D Pin 4OUT → Right Motor (-)
```

### Servo:
```
ESP8266 D6 (GPIO12) → Servo Signal
ESP8266 GND → Servo Ground
+3.3V (via regulator) → Servo Power
```

### Power:
```
Battery Pack (4xAA or similar) → L293D Power (Pin 8, +12V)
Battery Pack GND → L293D GND
USB Power Bank → ESP8266 5V input
Common GND between all components
```

## Available Commands in Web Interface

- **↑ Forward** - Both motors forward
- **↓ Backward** - Both motors backward
- **← Left** - Left motor reverse, right motor forward
- **→ Right** - Left motor forward, right motor reverse
- **■ Stop** - All motors stop
- **Speed Slider** - Adjust motor speed (0-255)
- **Steering Slider** - Adjust servo angle (0-180°)

## Troubleshooting Checklist

- [ ] ESP8266 connected to computer via USB
- [ ] Platform IO extension installed in VS Code
- [ ] WiFi SSID and password updated in main.cpp
- [ ] Serial port set correctly in platformio.ini
- [ ] All motor and servo pins are properly wired
- [ ] Battery is providing power to motors
- [ ] L293D is receiving proper voltage
- [ ] Servo has separate power supply or regulator

## Files in This Project

- **platformio.ini** - Build configuration
- **src/main.cpp** - Main firmware code with web interface
- **include/MotorControl.h** - Motor control header
- **README.md** - Full documentation

## Next Steps

Once the project is working:
1. Test motor controls individually
2. Calibrate servo center position if needed
3. Adjust speed values for balanced left/right movement
4. Consider adding additional features from the README
