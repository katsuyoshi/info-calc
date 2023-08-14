#include <Arduino.h>
#include <M5Unified.h>
#include <ESP32Servo.h>


typedef enum  {
  ServoStateInit,
  ServoStateOffA,
  ServoStateA,
  ServoStateOffB,
  ServoStateB,
} ServoState;



class Pusher {
  private:
    Servo _servo;
    int _pin_no;
    ServoState _state;
    ServoState _ex_state;
    int _adjust_angle;

    int angle() {
      switch(_state) {
        case ServoStateA:
          return 90 - 10 + _adjust_angle;
        case ServoStateB:
          return 90 + 10 + _adjust_angle;
        default:
          return 90 + _adjust_angle;
      }
    }

  public:
    Pusher(int pin_no, int adjust_angle = 0) {
      _pin_no = pin_no;
      _state = _ex_state = ServoStateInit;
      _adjust_angle = adjust_angle;
    }

    void begin() {
      _servo.setPeriodHertz(50);
      _servo.attach(_pin_no, 500, 2400);
      setState(ServoStateOffA);
    }

    void setState(ServoState state) {
      _state = state;
      if (_ex_state != _state) {
        _servo.write(angle());
        _ex_state = _state;
      }
    }

    void move_next() {
      _state = ServoState((int)_state + 1);
      switch(_state) {
        case ServoStateInit:
        case ServoStateOffA:
        case ServoStateOffB:
        case ServoStateA:
        case ServoStateB:
          break;
        default:
          _state = ServoStateOffA;
      }
      setState(_state);
    }

};


static Pusher pushers[] = {
  Pusher(22, 3),
  Pusher(19, 8),
  Pusher(23, 10),
};

void move_servos()
{
  for (int i = 0; i < 3; i++) {
    pushers[i].move_next();
  }
}

void setup() {

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setTextSize(3);
  M5.Display.setRotation(1);
  M5.Display.print("Hello World!!");
  Serial.println("Hello World!!"); 

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  for (int i = 0; i < 3; i++) {
    pushers[i].begin();
  }
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    move_servos();
  }
  delay(10);
}
