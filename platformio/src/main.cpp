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

enum class LColor {
    BLACK       = 0,
    BLUE        = 1,
    RED         = 2,
    PURPLE      = 3,
    GREEN       = 4,
    CYAN        = 5,
    YELLOW      = 6,
    WHITE       = 7,
};


class Light
{
private:
    int _b_pin;
    int _r_pin;
    int _g_pin;

public:

    typedef enum Color {

    };

    Light(int b_pin, int r_pin, int g_pin) {
        _b_pin = b_pin;
        _r_pin = r_pin;
        _g_pin = g_pin;
    }

    void begin() {
        pinMode(_r_pin, OUTPUT);
        pinMode(_g_pin, OUTPUT);
        pinMode(_b_pin, OUTPUT);
    }

    void set_color(LColor color) {

        switch (color) {
            case LColor::BLUE:
            case LColor::PURPLE:
            case LColor::CYAN:
            case LColor::WHITE:
                digitalWrite(_b_pin, HIGH);
                break;
            default:
                digitalWrite(_b_pin, LOW);
                break;
        }
        switch (color) {
            case LColor::RED:
            case LColor::PURPLE:
            case LColor::YELLOW:
            case LColor::WHITE:
                digitalWrite(_r_pin, HIGH);
                break;
            default:
                digitalWrite(_r_pin, LOW);
                break;
        }
        switch (color) {
            case LColor::GREEN:
            case LColor::CYAN:
            case LColor::YELLOW:
            case LColor::WHITE:
                digitalWrite(_g_pin, HIGH);
                break;
            default:
                digitalWrite(_g_pin, LOW);
                break;
        }

    }
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
        _on_time = 150;
        _off_time = 150;
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
    Pusher(22, 10, 10, 9),      // A: =, B: +
    Pusher(19, 16, 16, 9),      // A: ., B: 0
    Pusher(23, 14, 14, 11),     // A: 1, B: CA
    Pusher(33, 10, 10, 8),      // A:  , B: -
};

static Light light = Light(32, 26, 25);

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

        Add1Minute,
        Add10Minutes,

        Add1Hour,
        Add10Hours,
        Add100Hours,

        Sub1Minute,
        Sub10Minutes,

        Sub1Hour,
        Sub10Hours,
        Sub100Hours,
    } mode;

private:
    mode _mode = Unknown;
    int _value = 0;
    int _digit_values[4];
    char *_unit_pattern;

public:
    void set_time(int hour, int minute)
    {
        if (_mode == Unknown)
        {
            clear_all();
        }
        set_value((float)hour + (float)minute / 100.0);
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
        _mode = Clear;
        _value = 0;
        for (int i = 0; i < 4; i++) {
            _digit_values[i] = 0;
        }
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

#if defined(TEST_MODE) || defined(TEST_COUNT_UP_DOWN)
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

    void push_minus()
    {
        pushers[3].push_b();
        Serial.print("-");
    }

private:

    void add_ten_minutes(int times)
    {
        if (times == 0)
        {
            return;
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
            _value += 10;
        }
    }

    void sub_ten_minutes(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Sub10Minutes)
        {
            push_minus();
            push_dot();
            push_one();
            _mode = Sub10Minutes;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value -= 10;
        }
    }

    void add_one_minute(int times)
    {
        if (times == 0)
        {
            return;
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
            _value += 1;
        }
    }

    void sub_one_minute(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Sub1Minute)
        {
            push_minus();
            push_dot();
            push_zero();
            push_one();
            _mode = Sub1Minute;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value -= 1;
        }
    }

    void add_hundred_hours(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Add100Hours)
        {
            push_plus();
            push_one();
            push_zero();
            push_zero();
            _mode = Add100Hours;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value += 10000;
        }
    }

    void sub_hundred_hours(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Sub100Hours)
        {
            push_minus();
            push_one();
            push_zero();
            push_zero();
            _mode = Sub100Hours;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value -= 10000;
        }
    }

    void add_ten_hours(int times)
    {
        if (times == 0)
        {
            return;
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
            _value += 1000;
        }
    }

    void sub_ten_hours(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Sub10Hours)
        {
            push_minus();
            push_one();
            push_zero();
            _mode = Sub10Hours;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value -= 1000;
        }
    }

    void add_one_hour(int times)
    {
        if (times == 0)
        {
            return;
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
            _value += 100;
        }
    }

    void sub_one_hour(int times)
    {
        if (times == 0)
        {
            return;
        }
        if (_mode != Sub1Hour)
        {
            push_minus();
            push_one();
            _mode = Sub1Hour;
        }
        for (int i = 0; i < times; i++)
        {
            push_equal();
            _value -= 100;
        }
   }

    void set_digit(int number, int digit) {
        if (number == 0) { return; }

        bool sign = number >= 0;
        number = abs(number);
        if (number > 5) {
            number = 10 - number;
            sign = !sign;
        }

        switch (digit) {
            case 0:
                {
                    if (sign) {
                        add_one_minute(number);
                    } else {
                        sub_one_minute(number);
                    }
                }
                break;
            case 1:
                {
                    if (sign) {
                        add_ten_minutes(number);
                    } else {
                        sub_ten_minutes(number);
                   }
                }
                break;

            case 2:
                {
                    if (sign) {
                        add_one_hour(number);
                    } else {
                        sub_one_hour(number);
                    }
                }
                break;
            case 3:
                {
                    if (sign) {
                        add_ten_hours(number);
                    } else {
                        sub_ten_hours(number);
                    }
                }
                break;
            case 4:
                {
                    if (sign) {
                        add_hundred_hours(number);
                    } else {
                        sub_hundred_hours(number);
                    }
                }
                break;
            defualt:
                return;
        }
    }

#if defined(TEST_MODE) || defined(TEST_COUNT_UP_DOWN)
public:
#endif

    void set_value(float value) {
Serial.printf("set_value %.2f -> \t", value);
        int v = (value * 100 + 0.5);
        int base = 1;

        for (int digit = 0; digit < 5; digit++) {
            if (_value == v) break;
            int n = v - _value;
            n = (n / base) % 10;
            set_digit(n, digit);
            base *= 10;
        }
Serial.printf("\t-> _value %d, v: %d\n", _value, v);
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

    light.begin();
    light.set_color(LColor::WHITE);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    for (int i = 0; i < 4; i++)
    {
        pushers[i].begin();
    }

#if !defined(TEST_MODE) && !defined(TEST_COUNT_UP_DOWN)
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
#endif
}

#ifdef TEST_MODE
// test servo moving.
void test_mode() {
    if (M5.BtnA.wasPressed())
    {
        switch (test_state)
        {
        case 0:
            calc.push_equal();
            break;
        case 1:
            calc.push_plus();
            break;

        case 2:
            calc.push_dot();
            break;
        case 3:
            calc.push_zero();
            break;

        case 4:
            calc.push_one();
            break;
        case 5:
            calc.push_clear_all();
            break;

        case 6:
            calc.push_minus();
            break;

        default:
            test_state = 0;
            calc.push_clear_all();
        }
        test_state = (test_state + 1) % 7;
    }
}
#endif

#ifdef TEST_COUNT_UP_DOWN
void test_count_up_down() {
    delay(10000);
    Serial.println("test_count_up_down");

    calc.clear_all();
    for (int i = 0; i < 10; i++) {
        calc.set_value((float)i * 0.01);
        delay(1000);
    }
    for (int i = 0; i < 10; i++) {
        calc.set_value((float)i * 0.1);
        delay(1000);
    }
    for (int i = 0; i < 10; i++) {
        calc.set_value((float)i * 1.0);
        delay(1000);
    }
    for (int i = 0; i < 10; i++) {
        calc.set_value((float)i * 10.0);
        delay(1000);
    }

    calc.set_value(0.0);
    delay(1000);

    for (int i = 9; i >= 0; i--) {
        calc.set_value((float)i * 10.0);
        delay(1000);
    }
    for (int i = 9; i >= 0; i--) {
        calc.set_value((float)i * 1.0);
        delay(1000);
    }
    for (int i = 9; i >= 0; i--) {
        calc.set_value((float)i * 0.1);
        delay(1000);
    }
    for (int i = 9; i >= 0; i--) {
        calc.set_value((float)i * 0.01);
        delay(1000);
    }

    calc.set_value(0.0);
    delay(1000);

}
#endif

void loop()
{
    static int n = 0;
    static int test_state = 0;
    M5.update();

#ifdef TEST_MODE
    test_mode();
    return;
#endif
#ifdef TEST_COUNT_UP_DOWN
    test_count_up_down();
    return;
#endif

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

    if (M5.BtnA.wasReleaseFor(1000)) {
        current_channel = 0;
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
    delay(10);
}
