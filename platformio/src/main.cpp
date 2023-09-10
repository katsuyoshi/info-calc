/*
MIT License

Copyright (c) 2023 Katsuyoshi Ito

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <FastLED.h>
#include <time.h>
#include <esp_now.h>
#include "led.h"
#include "env.h"

// for LEDs
#define NUM_LEDS 25
#define LED_DATA_PIN 27
#define BRIGHTNESS  50

CRGB leds[NUM_LEDS];

// for ntp server

static const char *ntpServer = "ntp.nict.jp";
static const long gmtOffset_sec = 9 * 3600;
static const int daylightOffset_sec = 0;

static bool time_available = false;
static struct tm currentTime;


#define INVALID_DATA_INTERVAL   1 * 60 * 60 * 1000

// showing data rounding
#define ROUNDING_INTERVAL      30 * 1000

static bool rounding = false;
static unsigned long rounding_at = 0;


// for pusher
typedef enum
{
    ServoStateInit,
    ServoStateOffA,
    ServoStateA,
    ServoStateOffB,
    ServoStateB,
} ServoState;

struct ChannelValue {
    float value;
    unsigned long received_at;
    bool available;
    char unit[8];
};

class Pusher
{
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

    int angle()
    {
        switch (_state)
        {
        case ServoStateA:
            return 90 - _a_angle + _adjust_angle;
        case ServoStateB:
            return 90 + _b_angle + _adjust_angle;
        default:
            return 90 + _adjust_angle;
        }
    }

public:
    Pusher(int pin_no, int a_angle = 10, int b_angle = 10, int adjust_angle = 0)
    {
        _pin_no = pin_no;
        _state = _ex_state = ServoStateInit;
        _a_angle = a_angle;
        _b_angle = b_angle;
        _adjust_angle = adjust_angle;
        _on_time = 100;
        _off_time = 100;
    }

    void begin()
    {
        _servo.setPeriodHertz(50);
        _servo.attach(_pin_no, 500, 2400);
        setState(ServoStateOffA);
    }

    void setState(ServoState state)
    {
        _state = state;
        if (_ex_state != _state)
        {
            _servo.write(angle());
            _ex_state = _state;
        }
    }

    void move_next()
    {
        _state = ServoState((int)_state + 1);
        switch (_state)
        {
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

    void push_a()
    {
        setState(ServoStateA);
        delay(_on_time);
        setState(ServoStateOffA);
        delay(_off_time);
    }

    void push_b()
    {
        setState(ServoStateB);
        delay(_on_time);
        setState(ServoStateOffB);
        delay(_off_time);
    }
};

static Pusher pushers[] = {
    Pusher(22, 12, 10, 3),   // A: =, B: +
    Pusher(19, 13, 12, 8),  // A: ., B: 0
    Pusher(23, 15, 10, 12), // A: 1, B: CA
};

void move_servos()
{
    for (int i = 0; i < 3; i++)
    {
        pushers[i].move_next();
    }
}

// display time on a calculator.
class Calculator
{
    typedef enum
    {
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
    int _value = 0;
    char *_unit_pattern;

public:
    void set_time(int hour, int minute)
    {
        if (_mode == Unknown)
        {
            clear_all();
        }
        bool ret;
        ret = set_minute(minute);
        if (ret == false)
        {
            _mode = Unknown;
            set_time(hour, minute);
            return;
        }
        ret = set_hour(hour);
        if (ret == false)
        {
            _mode = Unknown;
            set_time(hour, minute);
            return;
        }
        set_unit("clock");
    }

    void set_channel_value(ChannelValue *channel_value) {
        if (channel_value->available == false) { return; }

        set_value(channel_value->value);
        set_unit(channel_value->unit);
    }

    void clear_all()
    {
        push_clear_all();
        push_equal();
        _hour = 0;
        _minute = 0;
        _mode = Clear;
        _value = 0;
        _unit_pattern = NULL;
    }

    void set_unit(const char *unit) {
        const char *units[] = {
            "clock",
            "°C",
            "%"
        };

        int num = sizeof(units) / sizeof(units[0]);
        const char **ptr = &units[0];
        char *pattern = NULL;

        for (int i = 0; i < num; i++) {
            if (strcmp(*ptr++, unit) == 0) {
                switch (i) {
                    case 0:
                        pattern = led_clock_pattern;
                        break;
                    case 1:
                        if (_value < 1000) {
                            pattern = led_cold_temperature;
                        } else
                        if (_value > 2500) {
                            pattern = led_hot_temperature;
                        } else {
                            pattern = led_norm_temperature;
                        }
                        break;
                    case 2:
                        if (_value < 3333) {
                            pattern = led_low_humidity;
                        } else
                        if (_value > 6666) {
                            pattern = led_high_humidity;
                        } else {
                            pattern = led_norm_humidity;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        if (pattern == NULL) { return; }
        if (pattern == _unit_pattern) { return; }
        _unit_pattern = pattern;
        Serial.println(_unit_pattern);

        char *ch = _unit_pattern;
        // Reverse the order of the set values for an upside-down arrangement.
        for (int i = NUM_LEDS - 1; i >= 0; i--) {
            switch (*ch++) {
                case 'R':
                    leds[i] = CRGB::Red;
                    break;
                case 'G':
                    leds[i] = CRGB::Green;
                    break;
                case 'B':
                    leds[i] = CRGB::Blue;
                    break;
                default:
                    leds[i] = CRGB::Black;
                    break;
            }
        }
        FastLED.show();
    }

#ifdef TEST_MODE
public:
#else
private:
#endif

    void push_clear_all()
    {
        pushers[2].push_b();
        Serial.print("C");
    }

    void push_one()
    {
        pushers[2].push_a();
        Serial.print("1");
    }

    void push_zero()
    {
        pushers[1].push_b();
        Serial.print("0");
    }

    void push_dot()
    {
        pushers[1].push_a();
        Serial.print(".");
    }

    void push_plus()
    {
        pushers[0].push_b();
        Serial.print("+");
    }

    void push_equal()
    {
        pushers[0].push_a();
        Serial.print("=");
    }

private:
    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_ten_minutes(int times)
    {
        if (times == 0)
        {
            return true;
        }
        if (_mode != Add10Minutes)
        {
            push_plus();
            push_dot();
            push_one();
            _mode = Add10Minutes;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _minute += 10;
        }
        _hour += _minute / 100;
        if (_hour >= 24)
        {
            return false;
        }
        _minute = _minute % 100;
        return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_one_minute(int times)
    {
        if (times == 0)
        {
            return true;
        }
        if (_mode != Add1Minute)
        {
            push_plus();
            push_dot();
            push_zero();
            push_one();
            _mode = Add1Minute;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _minute += 1;
        }
        _hour += _minute / 100;
        if (_hour >= 24)
        {
            return false;
        }
        _minute = _minute % 100;
        return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_ten_hours(int times)
    {
        if (times == 0)
        {
            return true;
        }
        if (_mode != Add10Hours)
        {
            push_plus();
            push_one();
            push_zero();
            _mode = Add10Hours;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _hour += 10;
        }
        return _hour < 24;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool add_one_hour(int times)
    {
        if (times == 0)
        {
            return true;
        }
        if (_mode != Add1Hour)
        {
            push_plus();
            push_one();
            _mode = Add1Hour;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _hour += 1;
        }
        return _hour < 24;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool set_minute(int minute)
    {
        int diff = minute - _minute;
        if (diff == 0)
        {
            return true;
        }
        if (diff < 0)
        {
            diff += 100;
        }
        bool ret;
        ret = add_ten_minutes(diff / 10);
        if (ret == false)
        {
            return false;
        }
        ret = add_one_minute(diff % 10);
        if (ret == false)
        {
            return false;
        }
        return true;
    }

    // @return: if the hour is over 24 hours return false, otherwise return true.
    bool set_hour(int hour)
    {
        int diff = hour - _hour;
        if (diff == 0)
        {
            return true;
        }
        if (diff < 0)
        {
            diff += 24;
        }
        bool ret;
        ret = add_ten_hours(diff / 10);
        if (ret == false)
        {
            return false;
        }
        ret = add_one_hour(diff % 10);
        if (ret == false)
        {
            return false;
        }
        return true;
    }

    void set_digit(int number, int digit) {
        if (number == 0) { return; }

        switch (digit) {
            case -2:
                push_plus();
                push_dot();
                push_zero();
                push_one();
                break;
            case -1:
                push_plus();
                push_dot();
                push_one();
                break;
            case 0:
                push_plus();
                push_one();
                break;
            case 1:
                push_plus();
                push_one();
                push_zero();
                break;
            defualt:
                return;
        }
        for (int i = 0; i < number; i++)
        {
            push_equal();
        }
    }

    void set_value(float value) {
        int v = (value * 100 + 0.5);
        if (v == _value) { return; }
        _value = v;

        int digit = -2;
        for (int i = 0; i < 4; i++) {
            set_digit(v % 10, digit++);
            v /= 10;
        }
    }
};

static Calculator calc = Calculator();

#define NUMBER_OF_CHANNEL       11

static int current_channel = 0;
static struct ChannelValue channel_values[NUMBER_OF_CHANNEL];

static esp_now_peer_info_t espnow_slave;
static bool espnow_setuped = false;

static void set_rounding(bool f, bool update = false) {
    if (update == false && rounding == f) { return; }
    
    rounding = f;
    if (update) {
        rounding_at = millis();
    } else {
        // for rounding immediately
        rounding_at = millis() - ROUNDING_INTERVAL;
    }
}

static void update_time()
{
    if (!getLocalTime(&currentTime))
    {
        Serial.println("Failed to obtain time");
        return;
    }
    time_available = true;
    Serial.println(&currentTime, "%Y %m %d %a %H:%M:%S");
}

static void espnow_on_data_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
    ushort ch = 0;
    float value = 0.0f;
    char unit[8] = { 0 };
    ChannelValue *channel_value;

    sscanf((const char *)data, "%hd,%f,%s", &ch, &value, unit);
    if (ch < 1 || ch >= NUMBER_OF_CHANNEL) {
        Serial.printf("The channell is %d. ", ch);
        Serial.println("The channel should 1 to 10.");
        return;
    }

    channel_value = &channel_values[ch];
    channel_value->available = true;
    channel_value->value = value;
    channel_value->received_at = millis();
    strncpy(channel_value->unit, unit, 8);

    Serial.printf("<< %s\n", data);
    Serial.printf("ch %d\n", ch);
    Serial.printf("value %5.1f\n", value);
    Serial.printf("unit %s %d\n", unit, strlen(unit));

    set_rounding(true);
}

// @refer: https://it-evo.jp/blog/blog-1397/
static void espnow_setup_if_needed()
{
    if (espnow_setuped) return;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() == ESP_OK)
    {
        Serial.println("ESPNow Init Success");
        espnow_setuped = true;
    }
    else
    {
        Serial.println("ESPNow Init Failed");
        espnow_setuped = false;
        ESP.restart();
        return;
    }

    memset(&espnow_slave, 0, sizeof(espnow_slave));
    for (int i = 0; i < 6; ++i)
    {
        espnow_slave.peer_addr[i] = (uint8_t)0xff;
    }

    esp_err_t addStatus = esp_now_add_peer(&espnow_slave);
    if (addStatus == ESP_OK)
    {
        Serial.println("Pair success");
    }
    esp_now_register_recv_cb(espnow_on_data_receive);
}

static void display() {
    if (current_channel == 0) {
        calc.set_time(currentTime.tm_hour, currentTime.tm_min);
    } else {
        calc.set_channel_value(&channel_values[current_channel]);
    }
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    for (int i = 0; i < NUMBER_OF_CHANNEL; i++) {
        channel_values[i].available = false;
        channel_values[i].value = 0.0f;
        channel_values[i].unit[0] = '\0';
    }
    // channel zero is for time.
    channel_values[0].available = true;

    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(leds, NUM_LEDS); // GRB ordering is assumed
    FastLED.setBrightness(BRIGHTNESS);

    leds[24] = CRGB::Red;
    FastLED.show();

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    for (int i = 0; i < 3; i++)
    {
        pushers[i].begin();
    }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.println("\nCONNECTED!");

    leds[24] = CRGB::Green;
    FastLED.show();

    set_rounding(false);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // WiFi.disconnect(true);
    // WiFi.mode(WIFI_OFF);
    delay(20);
}

void loop()
{
    static int n = 0;
    static int test_state = 0;
    M5.update();

#ifdef TEST_MODE
    // test servo moving.
    if (M5.BtnA.wasPressed())
    {
        switch (test_state)
        {
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

    // Set it invalid after one hour past
    unsigned long now = millis();
    bool needs_to_change_current_channel = false;
    for (int i = 1; i < NUMBER_OF_CHANNEL; i++) {
        ChannelValue *channel_value = &channel_values[i];

        if (channel_value->available &&
            (now - channel_value->received_at >= INVALID_DATA_INTERVAL)) {
            channel_value->available = false;
            Serial.println("Invalid data");
            if (i == current_channel) {
                needs_to_change_current_channel = true;
            }
        }
    }

    if (rounding &&
        (now - rounding_at >= ROUNDING_INTERVAL)) {
        needs_to_change_current_channel = true;
        set_rounding(true, true);
    }

    if (M5.BtnA.wasPressed() || needs_to_change_current_channel)
    {
        int ch = current_channel;
        for (int i = 0; i < NUMBER_OF_CHANNEL; i++) {
            ch = (ch + 1) % NUMBER_OF_CHANNEL;
            if (channel_values[ch].available) {
                break;
            }
        }
        if (current_channel == ch) { return; }

        current_channel = ch;
        Serial.printf("The current channel is %d\n", current_channel);
        // Quit the rounding mode if channel no is return to zero.
        if (current_channel == 0) {
            set_rounding(false);
        }
        calc.clear_all();
        display();
    }

    // update time
    if (++n >= (1000 / 10))
    {
        n = 0;
        update_time();
        if (time_available) {
            espnow_setup_if_needed();
        }
    }

    display();
#endif

    delay(10);
}
