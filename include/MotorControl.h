#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>

class MotorController {
private:
  int in1Pin, in2Pin, enPin;
  
public:
  MotorController(int in1, int in2, int en);
  void setSpeed(int speed); // 0-255
  void forward(int speed);
  void backward(int speed);
  void stop();
};

#endif
