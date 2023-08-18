#include <Arduino.h>
#include <M5Unified.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <FastLED.h>
#include <time.h>

#include "env.h"


// for LEDs
#define NUM_LEDS        25
#define LED_DATA_PIN    27

CRGB leds[NUM_LEDS];

// for ntp server

static const char* ntpServer = "ntp.nict.jp";
static const long gmtOffset_sec     = -1 * 3600;//9 * 3600;
static const int daylightOffset_sec = 0;

static bool time_available = false;
static struct tm currentTime;


// for pusher
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
    int _a_angle;
    int _b_angle;
    int _on_time;
    int _off_time;

    int angle() {
      switch(_state) {
        case ServoStateA:
          return 90 - _a_angle + _adjust_angle;
        case ServoStateB:
          return 90 + _b_angle + _adjust_angle;
        default:
          return 90 + _adjust_angle;
      }
    }

  public:
    Pusher(int pin_no, int a_angle = 10, int b_angle = 10, int adjust_angle = 0) {
      _pin_no = pin_no;
      _state = _ex_state = ServoStateInit;
      _a_angle = a_angle;
      _b_angle = b_angle;
      _adjust_angle = adjust_angle;
      _on_time = 100;
      _off_time = 100;
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

    void push_a() {
      setState(ServoStateA);
      delay(_on_time);
      setState(ServoStateOffA);
      delay(_off_time);
    }

    void push_b() {
      setState(ServoStateB);
      delay(_on_time);
      setState(ServoStateOffB);
      delay(_off_time);
    }
};

static Pusher pushers[] = {
  Pusher(22, 11,  8,  3),    // A: =, B: +
  Pusher(19, 12, 12,  8),    // A: ., B: 0
  Pusher(23, 14, 10, 12),    // A: 1, B: CA
};

void move_servos()
{
  for (int i = 0; i < 3; i++) {
    pushers[i].move_next();
  }
}

// display time on a calculator.
class Calculator {
  typedef enum {
    Unknown,
    Clear,
    Add10Minutes,
    Add1Minute,
    Add10Hours,
    Add1Hour,
  } mode;

  private:
    mode _mode = Unknown;
    int _hour = 0;
    int _minute = 0;

  public:
    void setTime(int hour, int minute) {
      if (_mode == Unknown) {
        clear_all();
      }
      bool ret;
      ret = set_minute(minute);
      if (ret == false) {
        _mode = Unknown;
        setTime(hour, minute);
        return;
      }
      ret = set_hour(hour);
      if (ret == false) {
        _mode = Unknown;
        setTime(hour, minute);
        return;
      }
    }

    void clear_all() {
      push_clear_all();
      push_equal();
      _hour = 0;
      _minute = 0;
      _mode = Clear;
    }

#ifdef TEST_MODE
  public:
#else
  private:
#endif

    void push_clear_all() {
      int no = 2 * 5 + 1;
      leds[no] = CRGB::Green;
      pushers[2].push_b();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print("C");
    }

    void push_one() {
      int no = 2 * 5 + 0;
      leds[no] = CRGB::Green;
      FastLED.show();
      pushers[2].push_a();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print("1");
    }

    void push_zero() {
      int no = 1 * 5 + 1;
      leds[no] = CRGB::Green;
      FastLED.show();
      pushers[1].push_b();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print("0");
    }

    void push_dot() {
      int no = 1 * 5 + 0;
      leds[no] = CRGB::Green;
      FastLED.show();
      pushers[1].push_a();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print(".");
    }

    void push_plus() {
      int no = 0 * 5 + 1;
      leds[no] = CRGB::Green;
      FastLED.show();
      pushers[0].push_b();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print("+");
    }

    void push_equal() {
      int no = 0 * 5 + 0;
      leds[no] = CRGB::Green;
      FastLED.show();
      pushers[0].push_a();
      leds[no] = CRGB::Black;
      FastLED.show();
      Serial.print("=");
    }

  private:

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_ten_minutes(int times) {
      if (times == 0) { return true; }
      if (_mode != Add10Minutes) {
        push_plus();
        push_dot();
        push_one();
        _mode = Add10Minutes;
      }
      for (int i = 0; i < times; i++) {
        push_equal();
        _minute += 10;
      }
      _hour += _minute / 100;
      if (_hour >= 24) { return false; }
      _minute = _minute % 100;
      return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_one_minute(int times) {
      if (times == 0) { return true; }
      if (_mode != Add1Minute) {
        push_plus();
        push_dot();
        push_zero();
        push_one();
        _mode = Add1Minute;
      }
      for (int i = 0; i < times; i++) {
        push_equal();
        _minute += 1;
      }
      _hour += _minute / 100;
      if (_hour >= 24) { return false; }
      _minute = _minute % 100;
      return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_ten_hours(int times) {
      if (times == 0) { return true; }
      if (_mode != Add10Hours) {
        push_plus();
        push_one();
        push_zero();
        _mode = Add10Hours;
      }
      for (int i = 0; i < times; i++) {
        push_equal();
        _hour += 10;
      }
      return _hour < 24;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_one_hour(int times) {
      if (times == 0) { return true; }
      if (_mode != Add1Hour) {
        push_plus();
        push_one();
        _mode = Add1Hour;
      }
      for (int i = 0; i < times; i++) {
        push_equal();
        _hour += 1;
      }
      return _hour < 24;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool set_minute(int minute) {
      int diff = minute - _minute;
      if (diff == 0) { return true; }
      if (diff < 0) {
        diff += 100;
      }
      bool ret;
      ret = add_ten_minutes(diff / 10);
      if (ret == false) { return false; }
      ret = add_one_minute(diff % 10);
      if (ret == false) { return false; }
      return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool set_hour(int hour) {
      int diff = hour - _hour;
      if (diff == 0) { return true; }
      if (diff < 0) {
        diff += 24;
      }
      bool ret;
      ret = add_ten_hours(diff / 10);
      if (ret == false) { return false; }
      ret = add_one_hour(diff % 10);
      if (ret == false) { return false; }
      return true;
    }
};


static Calculator calc = Calculator();

void update_time()
{
  if (!getLocalTime(&currentTime)) {
    Serial.println("Failed to obtain time");
    return;
  }
  time_available = true;
  Serial.println(&currentTime, "%Y %m %d %a %H:%M:%S");
}

void setup() {

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setTextSize(3);
  M5.Display.setRotation(1);
  M5.Display.print("Hello World!!");
  Serial.println("Hello World!!"); 

  FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
  leds[24] = CRGB::Red;
  FastLED.show();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  for (int i = 0; i < 3; i++) {
    pushers[i].begin();
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      M5.Lcd.print(".");
  }
  M5.Lcd.println("\nCONNECTED!");

  leds[24] = CRGB::Green;
  FastLED.show();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //WiFi.disconnect(true);
  //WiFi.mode(WIFI_OFF);
  delay(20);
}

void loop() {
  static int n = 0;
  static int test_state = 0;
  M5.update();

#ifdef TEST_MODE
  // test servo moving.
  if (M5.BtnA.wasPressed()) {
    switch (test_state) {
      case 0:
        calc.push_clear_all();
        break;
      case 1:
        calc.push_plus();
        break;
      case 2:
        calc.push_zero();
        break;
      case 3:
        calc.push_dot();
        break;
      case 4:
        calc.push_one();
        break;
      case 5:
        calc.push_equal();
        break;
      default:
        test_state = 0;
        calc.push_clear_all();
    }
    test_state = (test_state + 1) % 6;
  }

#else

  if (M5.BtnA.wasPressed()) {
    calc.clear_all();
    delay(10000);
  }

  // update time
  if (++n >= (1000 / 10)) {
    n = 0;
    update_time();
  }
  if (time_available) {
    calc.setTime(currentTime.tm_hour, currentTime.tm_min);
  }

#endif

  delay(10);
}
