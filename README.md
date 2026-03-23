# RC Car Control Project with ESP8266

A WiFi-enabled RC car control system using:
- **ESP8266** microcontroller
- **L293D Motor Driver** for DC motor control
- **Servo Motor** for steering
- **Platform IO** for project management

## Hardware Requirements

### Components:
- ESP8266 D1 Mini microcontroller
- L293D Motor Driver IC
- 2x DC Motors (for propulsion)
- 1x Servo Motor (for steering)
- 4x AA Battery Holder (for motors)
- USB Power Bank (for ESP8266)
- Jumper wires
- Breadboard or PCB

### Wiring Diagram:

### Motor Control (L293D Shield):
```
Arduino Pin -> L293D Shield Pin
8  -> Motor A - Direction 1
9  -> Motor A - Direction 2
10 -> Motor A - Speed/Enable (PWM)

11 -> Motor B - Direction 1
12 -> Motor B - Direction 2
3  -> Motor B - Speed/Enable (PWM)

L293D Shield Outputs:
OUT1/OUT2 -> Motor A (Left Motor)
OUT3/OUT4 -> Motor B (Right Motor)
```

**Servo Connection:**
```
Arduino Pin 5 -> Servo Signal
GND -> Servo Ground
+5V (from shield) -> Servo Power
```

**Servo (Steering):**
```
ESP8266 D6 (GPIO12) -> Servo Signal
+3.3V -> Servo Power (through regulator)
GND -> Servo Ground
```

## Pin Configuration

The pin configuration is defined in [src/main.cpp](src/main.cpp):

- **Left Motor (A):** IN1=8, IN2=9, EN=10 (PWM)
- **Right Motor (B):** IN1=11, IN2=12, EN=3 (PWM)
- **Steering Servo:** pin 5

These are standard Arduino 0-13 pin numbers that correspond to your L293D shield.

## Software Setup

### Prerequisites:
1. Install [Platform IO](https://platformio.org/)
2. Install VS Code with Platform IO extension

### Configuration:

1. **Update WiFi Credentials:**
   Edit [src/main.cpp](src/main.cpp) and set:
   ```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
   ```

2. **Update Serial Monitor Port:**
   Edit [platformio.ini](platformio.ini):
   ```ini
   monitor_port = COMX  ; Change X to your COM port
   ```

3. **Build the Project:**
   ```bash
   platformio run --environment esp8266
   ```

4. **Upload to Device:**
   ```bash
   platformio run --environment esp8266 --target upload
   ```

5. **Monitor Output:**
   ```bash
   platformio device monitor --environment esp8266
   ```

## Usage

### Web Interface:
1. Connect ESP8266 to WiFi
2. Open a web browser and navigate to `http://<ESP8266_IP>`
3. Use the control panel to:
   - Move forward/backward
   - Turn left/right
   - Adjust speed (0-255)
   - Control steering angle (0-180°)

### Serial Commands:
Monitor the serial output (115200 baud) to see:
- WiFi connection status
- IP address
- Motor and servo status

## Motor Control Functions

- `moveForward(speed)` - Move both motors forward
- `moveBackward(speed)` - Move both motors backward
- `turnLeft(speed)` - Turn left (left motor reversed)
- `turnRight(speed)` - Turn right (right motor reversed)
- `stopMotors()` - Stop all motors
- `setSteering(angle)` - Set servo angle (0-180°)

## Libraries Used

- **ESP8266WiFi** - WiFi connectivity
- **ESP8266WebServer** - Web server for control interface
- **Servo** - Servo motor control
- **ArduinoJson** - JSON support for future enhancements

## Troubleshooting

### Motors not moving:
1. Check battery connections
2. Verify L293D wiring
3. Test motor power independently
4. Check PWM pin configuration

### Servo not responding:
1. Verify servo signal wire to D6
2. Check servo power supply
3. Ensure servo is powered separately if necessary

### WiFi connection issues:
1. Verify SSID and password
2. Check serial monitor for error messages
3. Ensure 2.4GHz WiFi network (ESP8266 doesn't support 5GHz)

### Web interface not loading:
1. Get ESP8266 IP from serial monitor
2. Ensure device is on same network
3. Try resetting the ESP8266

## Future Enhancements

- [ ] Bluetooth control option
- [ ] Obstacle detection (ultrasonic sensor)
- [ ] Line following capability
- [ ] Speed feedback from encoders
- [ ] Mobile app control
- [ ] Battery voltage monitoring

## License

This project is provided as-is for educational purposes.
