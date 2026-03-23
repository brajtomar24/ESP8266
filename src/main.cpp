#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Servo.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>

/* ================= WiFi ================= */
const char* ssid = "A-2208";
const char* password = "9582272188";

IPAddress local_IP(192,168,1,150);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,0,0);

/* ================= PINS ================= */
// Single motor control - D0/D1 for forward/backward (reversed)
#define MOTOR_FWD D0   // Motor forward (moved from D1)
#define MOTOR_BWD D1   // Motor backward (moved from D2)

// Steering motor control (DRV8833)
#define STEERING_MOTOR_FWD D3  // B1 on DRV8833 (steering forward/left)
#define STEERING_MOTOR_BWD D4  // B2 on DRV8833 (steering backward/right)

// Lights
#define FRONT_LIGHT D7   // Front light
#define BACK_LIGHT D8    // Back light (moved from A0 to D8)

// RGB LED strip
#define RGB_PIN D6       // RGB strip data pin (moved from D5 to D6)

// Motor driver control
#define STBY D2         // Standby pin (moved from D7)

// RGB LED strip variables
#include <Adafruit_NeoPixel.h>
#define NUM_PIXELS 8     // Number of LEDs in strip
Adafruit_NeoPixel strip(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// Ultrasonic sensor pins
#define TRIG_PIN D5      // Trigger pin for ultrasonic sensor (moved from D6)
#define ECHO_PIN A0      // Echo pin for ultrasonic sensor (moved from D8 to A0)

// Obstacle detection settings
#define OBSTACLE_DISTANCE 25  // Stop if obstacle within 25cm (more sensitive)
#define CHECK_INTERVAL 70     // Check every 70ms (balanced response)

// Function declarations
void setRGBColor(int r, int g, int b);
void setRGBPattern(String pattern);
int speedToPWM(int s);
void stopMotors();
void forward();
void backward();
void stopSteering();
void steerLeft();
void steerRight();
long getUltrasonicDistance();
void checkObstacle();
void broadcastStatus();
void sendStatusToClient(uint8_t num);
void handleWebSocketStatus(uint8_t num, uint8_t * payload, size_t length);
void handleStatus();
void handleRoot();
void handleCmd();
void handleSpeed();
void handleSteer();
void handleLight();
void handleRGB();

/* ================= STATE ================= */
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

char moveCmd = 'S';
char lastMoveCmd = 'S';

int speedVal = 160;          // 0–255 from UI
int pwmVal   = 0;            // 0–1023 actual PWM
volatile bool speedChanged = false;

// Status variables for HTTP endpoint
String currentStatus = "Ready";
String currentDirection = "Stopped";
String currentSpeed = "160 km/h";

// WebSocket event handlers
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WEBSOCKET] [%u] Disconnected!\n", num);
      // Send status update to remaining clients
      broadcastStatus();
      break;
    case WStype_ERROR:
      Serial.printf("[WEBSOCKET] [%u] Error!\n", num);
      break;
    case WStype_TEXT:
      // Handle incoming WebSocket text messages
      Serial.printf("[WEBSOCKET] [%u] Received: %s\n", num, payload);
      
      // Parse JSON commands
      if (length > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
          Serial.printf("[WEBSOCKET] JSON parsed successfully\n");
          
          // Handle movement commands
          if (doc["cmd"].is<const char*>()) {
            String cmd = doc["cmd"].as<String>();
            if (cmd.length() > 0) {
              moveCmd = cmd[0];
              Serial.printf("[WEBSOCKET] Movement command: %c\n", moveCmd);
            }
          }
          
          // Handle speed commands
          if (doc["speed"].is<int>()) {
            speedVal = doc["speed"].as<int>();
            Serial.printf("[WEBSOCKET] About to call speedToPWM with speed: %d\n", speedVal);
            pwmVal = speedToPWM(speedVal);
            speedChanged = true;
            Serial.printf("[WEBSOCKET] Speed: %d PWM: %d\n", speedVal, pwmVal);
          } else {
            Serial.printf("[WEBSOCKET] No speed command found or invalid type\n");
          }
          
          // Handle steering commands
          if (doc["steer"].is<int>() || doc["steer"].is<const char*>()) {
            int angle = 0; // Initialize to prevent warning
            bool handled = false; // Track if steering was handled
            
            if (doc["steer"].is<int>()) {
              // Direct numeric angle (legacy support)
              angle = doc["steer"].as<int>();
              Serial.printf("[WEBSOCKET] Steering angle (numeric): %d\n", angle);
              handled = true;
            } else {
              // String command - preferred method
              String steerCmd = doc["steer"].as<String>();
              Serial.printf("[WEBSOCKET] Steering command (string): %s\n", steerCmd.c_str());
              
              if (steerCmd == "L") {
                steerLeft();
                handled = true;
              } else if (steerCmd == "R") {
                steerRight();
                handled = true;
              } else if (steerCmd == "C" || steerCmd == "S") {
                stopSteering();
                handled = true;
              } else {
                // Try to convert string to number (legacy support)
                angle = steerCmd.toInt();
                Serial.printf("[WEBSOCKET] Steering angle (from string): %d\n", angle);
                handled = true;
              }
            }
            
            // Handle numeric angle only if it wasn't handled as a simple command
            if (handled && doc["steer"].is<int>()) {
              if (angle >= 30 && angle <= 150) {
                if (angle < 70) {
                  steerLeft();
                } else if (angle > 110) {
                  steerRight();
                } else {
                  stopSteering();
                }
              }
            } else if (handled && doc["steer"].is<const char*>()) {
              // Only handle numeric from string if it wasn't L/R/C
              String steerCmd = doc["steer"].as<String>();
              if (steerCmd != "L" && steerCmd != "R" && steerCmd != "C" && steerCmd != "S") {
                if (angle >= 30 && angle <= 150) {
                  if (angle < 70) {
                    steerLeft();
                  } else if (angle > 110) {
                    steerRight();
                  } else {
                    stopSteering();
                  }
                }
              }
            }
          } else {
            Serial.printf("[WEBSOCKET] No steering command found - type: %s, value: %s\n", 
                         doc["steer"].is<int>() ? "int" : doc["steer"].is<const char*>() ? "string" : "unknown",
                         doc["steer"].is<const char*>() ? doc["steer"].as<String>().c_str() : "N/A");
          }
          
          // Handle light commands
          if (doc["light"].is<const char*>()) {
            String lightCmd = doc["light"].as<String>();
            if (lightCmd == "F") {
              digitalWrite(FRONT_LIGHT, HIGH);
              digitalWrite(BACK_LIGHT, LOW);
            } else if (lightCmd == "B") {
              digitalWrite(FRONT_LIGHT, LOW);
              digitalWrite(BACK_LIGHT, HIGH);
            } else if (lightCmd == "A") {
              digitalWrite(FRONT_LIGHT, HIGH);
              digitalWrite(BACK_LIGHT, HIGH);
            } else if (lightCmd == "O") {
              digitalWrite(FRONT_LIGHT, LOW);
              digitalWrite(BACK_LIGHT, LOW);
            }
          }
          
          // Handle RGB commands
          if (doc["rgb"].is<const char*>()) {
            String rgbCmd = doc["rgb"].as<String>();
            setRGBPattern(rgbCmd);
          }
          
          // Handle status update requests
          if (doc["status"].is<const char*>()) {
            currentStatus = doc["status"].as<String>();
            currentDirection = doc["direction"].is<const char*>() ? doc["direction"].as<String>() : currentDirection;
            currentSpeed = doc["speed"].is<const char*>() ? doc["speed"].as<String>() : currentSpeed;
            
            Serial.printf("[WEBSOCKET] Status updated: %s, Direction: %s, Speed: %s\n", 
                       currentStatus.c_str(), currentDirection.c_str(), currentSpeed.c_str());
            
            // Broadcast updated status to all connected clients
            broadcastStatus();
          }
        } else {
          Serial.printf("[WEBSOCKET] JSON parse error: %s\n", error.c_str());
        }
      }
      break;
    case WStype_CONNECTED:
      Serial.printf("[WEBSOCKET] [%u] Connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
      // Send current status to newly connected client
      sendStatusToClient(num);
      break;
    case WStype_BIN:
      Serial.printf("[WEBSOCKET] [%u] Binary data received\n", num);
      break;
    case WStype_FRAGMENT_TEXT_START:
      Serial.printf("[WEBSOCKET] [%u] Text fragment start\n", num);
      break;
    case WStype_FRAGMENT_BIN_START:
      Serial.printf("[WEBSOCKET] [%u] Binary fragment start\n", num);
      break;
    case WStype_FRAGMENT:
      Serial.printf("[WEBSOCKET] [%u] Fragment received\n", num);
      break;
    case WStype_FRAGMENT_FIN:
      Serial.printf("[WEBSOCKET] [%u] Fragment end\n", num);
      break;
    case WStype_PING:
      Serial.printf("[WEBSOCKET] [%u] Ping received\n", num);
      break;
    case WStype_PONG:
      Serial.printf("[WEBSOCKET] [%u] Pong received\n", num);
      break;
  }
}

void handleWebSocketStatus(uint8_t num, uint8_t * payload, size_t length) {
  // This function handles specific status requests if needed
  Serial.printf("[WEBSOCKET] Client [%u] status request\n", num);
  
  // Send current status to requesting client
  String response = "{\"status\":\"" + currentStatus + "\",\"direction\":\"" + currentDirection + "\",\"speed\":\"" + currentSpeed + "\"}";
  webSocket.sendTXT(num, response);
}

void handleStatus() {
  Serial.println("[STATUS] Status request received");
  
  // Create JSON response
  String jsonResponse = "{\"status\":\"" + currentStatus + "\",\"direction\":\"" + currentDirection + "\",\"speed\":\"" + currentSpeed + "\"}";
  
  server.send(200, "application/json", jsonResponse);
  Serial.print("[STATUS] Response sent: ");
  Serial.println(jsonResponse);
}

// WebSocket helper functions
void broadcastStatus() {
  String response = "{\"status\":\"" + currentStatus + "\",\"direction\":\"" + currentDirection + "\",\"speed\":\"" + currentSpeed + "\"}";
  webSocket.broadcastTXT(response);
  Serial.printf("[WEBSOCKET] Status broadcast: %s\n", response.c_str());
}

void sendStatusToClient(uint8_t num) {
  String response = "{\"status\":\"" + currentStatus + "\",\"direction\":\"" + currentDirection + "\",\"speed\":\"" + currentSpeed + "\"}";
  webSocket.sendTXT(num, response);
  Serial.printf("[WEBSOCKET] Status sent to client [%u]: %s\n", num, response.c_str());
}


// Obstacle detection variables
bool obstacleDetected = false;
unsigned long lastObstacleCheck = 0;
int currentDistance = 999;  // Current distance in cm

// Steering motor variables
unsigned long lastSteeringTime = 0;
#define STEERING_TIMEOUT 500  // Stop steering after 500ms of inactivity
bool steeringIsActive = false;  // Track if steering is currently active

// RGB chaser variables
unsigned long lastChaserUpdate = 0;
#define CHASER_SPEED 100  // Update chaser every 100ms
bool chaserActive = false;  // Track if chaser is active

// Function declarations
void setRGBColor(int r, int g, int b);
void setRGBPattern(String pattern);
void updateChaser();
int speedToPWM(int s);
void stopMotors();
void forward();
void backward();
void stopSteering();
void steerLeft();
void steerRight();
long getUltrasonicDistance();
void checkObstacle();
void broadcastStatus();
void sendStatusToClient(uint8_t num);
void handleWebSocketStatus(uint8_t num, uint8_t * payload, size_t length);
void handleStatus();
void handleRoot();
void handleCmd();
void handleSpeed();
void handleSteer();
void handleLight();
void handleRGB();


/* ================= HELPERS ================= */
int speedToPWM(int s) {
  Serial.printf("[PWM] Input speed: %d\n", s);
  
  // Handle zero speed case first
  if (s <= 0) {
    Serial.printf("[PWM] Zero speed, returning PWM: 0\n");
    return 0;  // Zero speed = zero PWM
  }
  
  // Apply minimum speed threshold
  if (s < 10) s = 10;              // Lower dead-zone for better control
  
  // Map 10-255 to 150-300 PWM range as requested
  int result = map(s, 10, 255, 160, 255);
  Serial.printf("[PWM] Mapped speed %d to PWM: %d\n", s, result);
  return result;
}

void stopMotors() {
  // Aggressive motor stop - force all pins LOW
  digitalWrite(MOTOR_FWD, LOW);
  digitalWrite(MOTOR_BWD, LOW);
  
  // Disable motor driver briefly to ensure stop
  digitalWrite(STBY, LOW);
  delay(5);
  digitalWrite(STBY, HIGH); // Re-enable for next command
  
  Serial.println("[MOTOR] EMERGENCY STOP! Pins: FWD=" + String(MOTOR_FWD) + " BWD=" + String(MOTOR_BWD) + " STBY=" + String(STBY));
}

// Steering motor control functions
void stopSteering() {
  digitalWrite(STEERING_MOTOR_FWD, LOW);
  digitalWrite(STEERING_MOTOR_BWD, LOW);
  steeringIsActive = false; // Update steering state
  Serial.printf("[STEERING] Stop - Pins: FWD=%d BWD=%d\n", STEERING_MOTOR_FWD, STEERING_MOTOR_BWD);
}

void steerLeft() {
  digitalWrite(STEERING_MOTOR_BWD, LOW);
  analogWrite(STEERING_MOTOR_FWD, 400); // Reduced PWM for steering
  lastSteeringTime = millis(); // Update last steering time
  steeringIsActive = true; // Update steering state
  Serial.printf("[STEERING] Left - Pins: FWD=%d BWD=%d PWM=400\n", STEERING_MOTOR_FWD, STEERING_MOTOR_BWD);
}

void steerRight() {
  digitalWrite(STEERING_MOTOR_FWD, LOW);
  analogWrite(STEERING_MOTOR_BWD, 400); // Reduced PWM for steering
  lastSteeringTime = millis(); // Update last steering time
  steeringIsActive = true; // Update steering state
  Serial.printf("[STEERING] Right - Pins: FWD=%d BWD=%d PWM=400\n", STEERING_MOTOR_FWD, STEERING_MOTOR_BWD);
}

// Ultrasonic sensor functions
long getUltrasonicDistance() {
  long duration, distance;
  
  // Clear the trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send 10 microsecond pulse to trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Wait for echo pin to go HIGH, then measure the duration
  // Use reasonable timeout for good range without excessive delay
  duration = pulseIn(ECHO_PIN, HIGH, 50000); // 50ms timeout = ~4.3m max range
  
  // Calculate distance in cm
  // Sound speed = 343 m/s = 0.0343 cm/μs
  // Distance = duration * speed / 2 (round trip)
  distance = duration * 0.01715;
  
  // Debug output
  Serial.print("[DEBUG] Raw duration: ");
  Serial.print(duration);
  Serial.print("μs, Calculated distance: ");
  Serial.print(distance);
  Serial.println("cm");
  
  // Validate the reading
  if (duration == 0) {
    Serial.println("[ERROR] No echo received (timeout)");
    return -1;
  }
  
  if (distance < 2 || distance > 500) {
    Serial.print("[ERROR] Invalid distance: ");
    Serial.print(distance);
    Serial.println("cm (out of range 2-500cm)");
    return -1;
  }
  
  return distance;
}

void checkObstacle() {
  unsigned long currentTime = millis();
  
  // Check for obstacles at specified interval
  if (currentTime - lastObstacleCheck >= CHECK_INTERVAL) {
    lastObstacleCheck = currentTime;
    
    Serial.println("[TEST] === Starting ultrasonic measurement ===");
    
    // Test pin states before measurement
    int trigState = digitalRead(TRIG_PIN);
    int echoState = digitalRead(ECHO_PIN);
    Serial.print("[TEST] Pin states - TRIG: ");
    Serial.print(trigState);
    Serial.print(", ECHO: ");
    Serial.println(echoState);
    
    currentDistance = getUltrasonicDistance();
    
    // Debug output every check
    Serial.print("[ULTRASONIC] Final distance: ");
    Serial.print(currentDistance);
    Serial.println("cm");
    
    // Test range indicator
    if (currentDistance > 0) {
      if (currentDistance > 100) {
        Serial.println("[RANGE] Long distance detected (>100cm)");
      } else if (currentDistance > 50) {
        Serial.println("[RANGE] Medium distance detected (50-100cm)");
      } else if (currentDistance > 25) {
        Serial.println("[RANGE] Short distance detected (25-50cm)");
      } else if (currentDistance > 10) {
        Serial.println("[RANGE] Very short distance detected (10-25cm)");
      } else {
        Serial.println("[RANGE] Critical distance detected (<10cm)");
      }
      
      // Test with a known object
      Serial.println("[TEST] Try placing hand at 20cm, 50cm, 100cm");
    } else {
      Serial.println("[RANGE] No valid reading");
    }
    
    Serial.println("[TEST] === Measurement complete ===");
    
    // Check if obstacle is too close (ignore error readings)
    if (currentDistance > 0 && currentDistance < OBSTACLE_DISTANCE) {
      if (!obstacleDetected) {
        obstacleDetected = true;
        Serial.print("[OBSTACLE] Detected at ");
        Serial.print(currentDistance);
        Serial.println("cm - EMERGENCY STOP!");
        
        // Emergency stop
        stopMotors();
        moveCmd = 'S'; // Force stop command
        lastMoveCmd = 'S'; // Update last command to prevent re-execution
        
        // Flash RGB LED red to indicate obstacle (but don't override)
        setRGBColor(255, 0, 0);
        delay(100);
        setRGBColor(0, 0, 0);
        delay(100);
        setRGBColor(255, 0, 0);
      }
    } else {
      if (obstacleDetected && currentDistance > 0) {
        obstacleDetected = false;
        Serial.print("[OBSTACLE] Cleared - Distance: ");
        Serial.print(currentDistance);
        Serial.println("cm");
        
        // Clear obstacle indication
        setRGBColor(0, 255, 0); // Green to indicate clear
        delay(200);
        // Don't restore pattern - keep green to show it's clear
      }
    }
  }
}

void forward() {
  // Temporarily bypass obstacle check for testing
  // TODO: Re-enable obstacle detection when ultrasonic sensor is working
  /*
  if (obstacleDetected) {
    Serial.println("[MOTOR] Forward blocked by obstacle!");
    stopMotors();
    return;
  }
  */
  
  Serial.printf("[MOTOR] Forward called - speedVal: %d, pwmVal: %d\n", speedVal, pwmVal);
  digitalWrite(MOTOR_BWD, LOW);
  analogWrite(MOTOR_FWD, pwmVal);
  Serial.println("[MOTOR] Forward - PWM: " + String(pwmVal));
}

void backward() {
  digitalWrite(MOTOR_FWD, LOW);
  analogWrite(MOTOR_BWD, pwmVal);
  Serial.println("[MOTOR] Backward - PWM: " + String(pwmVal));
}

// RGB LED strip functions
void setRGBColor(int r, int g, int b) {
  for(int i=0; i<NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  Serial.print("[RGB] Color set: R=");
  Serial.print(r);
  Serial.print(" G=");
  Serial.print(g);
  Serial.print(" B=");
  Serial.println(b);
}

void setRGBPattern(String pattern) {
  if (pattern == "rainbow") {
    // Start LED chaser effect
    chaserActive = true;
    updateChaser(); // Call it immediately to start the animation
    lastChaserUpdate = millis(); // Reset the timer
    Serial.println("[RGB] Chaser pattern started");
  } else if (pattern == "off") {
    chaserActive = false;
    strip.clear();
    strip.show();
    Serial.println("[RGB] Off");
  } else if (pattern == "red") {
    chaserActive = false;
    setRGBColor(255, 0, 0);
  } else if (pattern == "green") {
    chaserActive = false;
    setRGBColor(0, 255, 0);
  } else if (pattern == "blue") {
    chaserActive = false;
    setRGBColor(0, 0, 255);
  } else if (pattern == "white") {
    chaserActive = false;
    setRGBColor(255, 255, 255);
  }
}

void updateChaser() {
  static int chaserPos = 0;
  static int colorCycle = 0;
  
  // Define colors for the cycle
  uint32_t colors[] = {
    strip.Color(255, 0, 0),   // Red
    strip.Color(0, 255, 0),   // Green  
    strip.Color(0, 0, 255),   // Blue
    strip.Color(255, 255, 0), // Yellow
    strip.Color(255, 0, 255), // Magenta
    strip.Color(0, 255, 255), // Cyan
    strip.Color(255, 128, 0), // Orange
    strip.Color(128, 0, 255)  // Purple
  };
  int numColors = sizeof(colors) / sizeof(colors[0]);
  
  strip.clear(); // Clear all pixels first
  
  // Set the chaser pixel with current color
  strip.setPixelColor(chaserPos, colors[colorCycle]);
  
  // Set trailing pixels (dimmer fade of same color)
  for(int i = 1; i <= 3; i++) {
    int trailPos = (chaserPos - i + NUM_PIXELS) % NUM_PIXELS;
    
    // Extract RGB components from current color
    uint32_t color = colors[colorCycle];
    uint8_t r = (color >> 16) & 255;
    uint8_t g = (color >> 8) & 255;
    uint8_t b = color & 255;
    
    // Fade the color
    uint8_t fadeR = r / (i + 1);
    uint8_t fadeG = g / (i + 1);
    uint8_t fadeB = b / (i + 1);
    
    strip.setPixelColor(trailPos, strip.Color(fadeR, fadeG, fadeB));
  }
  
  strip.show();
  chaserPos = (chaserPos + 1) % NUM_PIXELS; // Move to next position
  
  // Change color when we complete a full rotation
  if (chaserPos == 0) {
    colorCycle = (colorCycle + 1) % numColors;
    Serial.printf("[RGB] Chaser color changed to: %d\n", colorCycle);
  }
}

/* ================= WEB ================= */
void handleRoot() {
  Serial.println("[WEB] Root request received");
  
  // Serve separate HTML file with proper connection handling
  File htmlFile = LittleFS.open("/index.html", "r");
  if (!htmlFile) {
    Serial.println("[LITTLEFS] HTML file not found, listing files:");
    File root = LittleFS.open("/", "r");
    File file = root.openNextFile();
    while(file) {
      Serial.print("  FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
    }
    server.send(404, "text/plain", "HTML file not found");
    return;
  }
  
  Serial.println("[LITTLEFS] Serving HTML file successfully");
  
  // Set proper headers for caching and connection handling
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.sendHeader("Connection", "close");
  
  // Stream file directly instead of reading into String
  server.streamFile(htmlFile, "text/html");
  htmlFile.close();
  
  Serial.println("[WEB] HTML file sent successfully");
}

void handleCmd() {
  if (!server.hasArg("c")) {
    server.send(400, "text/plain", "Missing");
    return;
  }
  moveCmd = server.arg("c")[0];
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (!server.hasArg("s")) {
    server.send(400, "text/plain", "Missing");
    return;
  }

  speedVal = server.arg("s").toInt();
  // Fixed mapping: lower UI value = lower PWM (correct direction)
  pwmVal = map(speedVal, 0, 255, 200, 1023);  // 0->0, 255->1023
  speedChanged = true;

  Serial.print("[SPEED] ");
  Serial.print(speedVal);
  Serial.print(" PWM=");
  Serial.print(pwmVal);
  Serial.println(" (3V fixed)");

  server.send(200, "text/plain", "OK");
}

void handleSteer() {
  if (!server.hasArg("s")) {
    server.send(400, "text/plain", "Missing");
    return;
  }

  String steerCmd = server.arg("s");
  
  Serial.print("[STEER] Received command: ");
  Serial.print(steerCmd);
  Serial.print(" (length: ");
  Serial.print(steerCmd.length());
  Serial.println(")");
  
  // Motor-based steering control
  if (steerCmd == "L") {
    steerLeft();
  } else if (steerCmd == "R") {
    steerRight();
  } else if (steerCmd == "C" || steerCmd == "S") {
    stopSteering();
  } else {
    // Check if it's a numeric angle command (-90 to 90)
    int angle = steerCmd.toInt();
    if (angle >= -90 && angle <= 90) {
      if (angle < -20) {
        steerLeft();
      } else if (angle > 20) {
        steerRight();
      } else {
        stopSteering(); // Center zone
      }
      Serial.print("[STEER] Angle ");
      Serial.print(angle);
      Serial.println(" converted to motor control");
    } else {
      Serial.println("[STEER] Invalid command");
      server.send(400, "text/plain", "Invalid");
      return;
    }
  }
  
  server.send(200, "text/plain", "OK");
}

void handleLight() {
  if (!server.hasArg("l")) {
    server.send(400, "text/plain", "Missing");
    return;
  }

  String lightCmd = server.arg("l");
  
  if (lightCmd == "F") {
    // Front light on
    digitalWrite(FRONT_LIGHT, HIGH);
    digitalWrite(BACK_LIGHT, LOW);
    Serial.println("[LIGHT] Front ON");
  } else if (lightCmd == "B") {
    // Back light on
    digitalWrite(FRONT_LIGHT, LOW);
    digitalWrite(BACK_LIGHT, HIGH);
    Serial.println("[LIGHT] Back ON");
  } else if (lightCmd == "A") {
    // All lights on
    digitalWrite(FRONT_LIGHT, HIGH);
    digitalWrite(BACK_LIGHT, HIGH);
    Serial.println("[LIGHT] All ON");
  } else if (lightCmd == "O") {
    // All lights off
    digitalWrite(FRONT_LIGHT, LOW);
    digitalWrite(BACK_LIGHT, LOW);
    Serial.println("[LIGHT] All OFF");
  } else {
    server.send(400, "text/plain", "Invalid");
    return;
  }

  server.send(200, "text/plain", "OK");
}

void handleRGB() {
  if (!server.hasArg("p")) {
    server.send(400, "text/plain", "Missing");
    return;
  }

  String pattern = server.arg("p");
  setRGBPattern(pattern);
  
  server.send(200, "text/plain", "OK");
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] Single Motor RC Car");
  
  // Initialize LittleFS
  LittleFS.begin();
  Serial.println("[LITTLEFS] File system initialized");
  
  // Motor pins
  pinMode(MOTOR_FWD, OUTPUT);
  pinMode(MOTOR_BWD, OUTPUT);
  
  // Motor driver control
  pinMode(STBY, OUTPUT);
  
  // Light pins
  pinMode(FRONT_LIGHT, OUTPUT);
  pinMode(BACK_LIGHT, OUTPUT);
  
  // Initialize ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println("[ULTRASONIC] Sensor initialized");
  
  // Set PWM frequency for motor control
  analogWriteFreq(4000); // 4kHz PWM frequency
  
  // Initialize PWM value from default speed
  pwmVal = speedToPWM(speedVal);
  Serial.printf("[INIT] Default speed: %d, PWM: %d\n", speedVal, pwmVal);
  
  // Test the speedToPWM function with new range 150-300
  Serial.printf("[TEST] speedToPWM(0) = %d\n", speedToPWM(0));
  Serial.printf("[TEST] speedToPWM(1) = %d\n", speedToPWM(1));
  Serial.printf("[TEST] speedToPWM(10) = %d\n", speedToPWM(10));
  Serial.printf("[TEST] speedToPWM(127) = %d\n", speedToPWM(127));
  Serial.printf("[TEST] speedToPWM(255) = %d\n", speedToPWM(255));
  
  // Initialize RGB LED strip
  strip.begin();
  strip.show();
  
  // Initialize steering motor pins (DRV8833)
  pinMode(STEERING_MOTOR_FWD, OUTPUT);
  pinMode(STEERING_MOTOR_BWD, OUTPUT);
  digitalWrite(STEERING_MOTOR_FWD, LOW);
  digitalWrite(STEERING_MOTOR_BWD, LOW);
  Serial.println("[STEERING] Motor initialized");
  
  // Enable motor driver
  digitalWrite(STBY, HIGH);
  
  // Setup WiFi
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] Connected: ");
  Serial.println(WiFi.localIP());
  
  // Setup HTTP server with proper connection handling
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/speed", handleSpeed);
  server.on("/steer", handleSteer);
  server.on("/light", handleLight);
  server.on("/rgb", handleRGB);
  server.on("/status", handleStatus);  // Add status endpoint
  
  // Setup WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);  // Register event handler
  Serial.println("[WEBSOCKET] Server started on port 81");
  Serial.println("[WEBSOCKET] Event handler registered");
  
  // Configure server for better connection handling
  server.keepAlive(false);  // Disable keep-alive to prevent hanging connections
  
  server.begin();
  
  Serial.println("[HTTP] Server started on port 80");
  Serial.println("[HTTP] Keep-alive: disabled");
  Serial.println("[INFO] For tilt control: Use ngrok or setup HTTPS");
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();
  webSocket.loop();  // Handle WebSocket connections
  delay(10); // Small delay to prevent overwhelming server
  
  // Check for obstacles continuously
  // checkObstacle();
  
  // Check for steering timeout - stop steering motor if inactive
  if (steeringIsActive && (millis() - lastSteeringTime > STEERING_TIMEOUT)) {
    stopSteering();
    Serial.println("[STEERING] Auto-stop due to inactivity");
  }
  
  // Update RGB chaser animation
  if (chaserActive && (millis() - lastChaserUpdate > CHASER_SPEED)) {
    updateChaser();
    lastChaserUpdate = millis();
  }
  
  // Temporarily disable additional safety check for testing
  // TODO: Re-enable when obstacle detection is working
  /*
  // Additional safety: Force stop if obstacle detected and car is trying to move forward
  if (obstacleDetected && (moveCmd == 'F' || moveCmd == 'f')) {
    Serial.println("[SAFETY] Emergency stop - obstacle detected!");
    stopMotors();
    moveCmd = 'S';
    lastMoveCmd = 'S';
    
    // Update status and broadcast to WebSocket clients
    currentStatus = "Obstacle Detected!";
    currentDirection = "Stopped";
    broadcastStatus();
    
    // Force motor pins LOW to ensure stop
    digitalWrite(MOTOR_FWD, LOW);
    digitalWrite(MOTOR_BWD, LOW);
    digitalWrite(STBY, LOW); // Disable motor driver
    delay(60); // Reduced from 200ms to 60ms for faster response
    digitalWrite(STBY, HIGH); // Re-enable motor driver
  }
  */

  if (moveCmd != lastMoveCmd || speedChanged) {
    Serial.printf("[LOOP] Processing command - moveCmd: %c, lastMoveCmd: %c, speedChanged: %d\n", 
                   moveCmd, lastMoveCmd, speedChanged);
    
    // Re-apply current movement command with new speed if only speed changed
    if (speedChanged && moveCmd == lastMoveCmd) {
      Serial.printf("[LOOP] Speed only change, re-applying: %c\n", moveCmd);
    }
    
    switch (moveCmd) {
      case 'F': 
        forward(); 
        currentStatus = "Moving Forward";
        currentDirection = "Moving";
        break;
      case 'B': 
        backward(); 
        currentStatus = "Moving Backward";
        currentDirection = "Moving";
        break;
      case 'L': 
        steerLeft(); 
        currentStatus = "Turning Left";
        break;
      case 'R': 
        steerRight(); 
        currentStatus = "Turning Right";
        break;
      case 'S': 
        stopMotors(); 
        currentStatus = "Stopped";
        currentDirection = "Stopped";
        break;
      default: 
        stopMotors(); 
        currentStatus = "Ready";
        currentDirection = "Stopped";
        break;
    }
    
    // Update speed display
    currentSpeed = String(speedVal) + " km/h";
    
    // Broadcast status to all WebSocket clients
    broadcastStatus();
    
    lastMoveCmd = moveCmd;
    speedChanged = false;
  }

  yield();
}
