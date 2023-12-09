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
#include "pusher.h"
#include "light.h"
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

// Light pattern
enum LightPattern {
    LIGHT_OFF,
    LIGHT_NORMAL,
    LIGHT_JUST_HOUR,
    LIGHT_TIMER,
    LIGHT_LESS_ONE_MINITUE,
    LIGHT_LESS_THIRTY_SECONDS,
    LIGHT_LESS_TEN_SECONDS,
    LIGHT_LESS_FIVE_SECONDS,
    LIGHT_BANG,
    LIGHT_THREE_FEAVER,
    LIGHT_FOUR_FEVER,
};


struct ChannelValue {
    float value;
    unsigned long received_at;
    bool available;
    char unit[16];
};

static Pusher pushers[] = {
    Pusher(22, 11, 10, 9),      // A: =, B: +
    Pusher(19, 16, 17, 9),      // A: ., B: 0
    Pusher(23, 16, 16, 11),     // A: 1, B: CA
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

    typedef enum
    {
        UnitUnknown,
        UnitClock,
        UnitTimer,
        UnitTemperature,
        UnitHumidity,
    } unit_type;

private:
    mode _mode = Unknown;
    int _value = 0;
    int _digit_values[4];
    char *_unit_pattern;
    unit_type _unit = UnitClock;
    LightPattern _light_pattern = LIGHT_NORMAL;

public:

    unit_type unit() { return _unit; }
    LightPattern light_pattern() { return _light_pattern; }
    void set_light_pattern(LightPattern pat) { _light_pattern = pat; }

    void set_time(int hour, int minute)
    {
        if (_mode == Unknown)
        {
            clear_all();
        }
        set_unit("clock");
        set_value((float)hour + (float)minute / 100.0);
    }

    void set_channel_value(ChannelValue *channel_value) {
        if (channel_value->available == false) { return; }

        set_unit(channel_value->unit);
        set_value(channel_value->value);
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
            "timer",
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
                        _unit = UnitClock;
                        break;
                    case 1:
                        pattern = led_timer_pattern;
                        _unit = UnitTimer;
                        break;
                    case 2:
                        if (_value < 1000) {
                            pattern = led_cold_temperature;
                        } else
                        if (_value > 2500) {
                            pattern = led_hot_temperature;
                        } else {
                            pattern = led_norm_temperature;
                        }
                        _unit = UnitTemperature;
                        break;
                    case 3:
                        if (_value < 3333) {
                            pattern = led_low_humidity;
                        } else
                        if (_value > 6666) {
                            pattern = led_high_humidity;
                        } else {
                            pattern = led_norm_humidity;
                        }
                        _unit = UnitHumidity;
                        break;
                    default:
                        _unit = UnitUnknown;
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
        int v = (value * 100 + 0.5);
        if (_value == v) return;

Serial.printf("set_value %.2f -> \t", value);

        int base = 1;

        for (int digit = 0; digit < 5; digit++) {
            if (_value == v) break;
            int n = v - _value;
            n = (n / base) % 10;
            set_digit(n, digit);
            base *= 10;
        }
Serial.printf("\t-> _value %d, v: %d\n", _value, v);

Serial.printf("unit %d\n", _unit);
        switch(_unit) {
        case UnitTimer:
            if (_value >= 100) {
                _light_pattern = LIGHT_TIMER;
            } else
            if (_value >= 30) {
                _light_pattern = LIGHT_LESS_ONE_MINITUE;
            } else
            if (_value >= 10) {
                _light_pattern = LIGHT_LESS_THIRTY_SECONDS;
            } else
            if (_value >= 5) {
                _light_pattern = LIGHT_LESS_TEN_SECONDS;
            } else
            if (_value > 0) {
                _light_pattern = LIGHT_LESS_FIVE_SECONDS;
            } else {
                _light_pattern = LIGHT_FOUR_FEVER;
            }
            break;

        case UnitClock:
            if ((_value % 100 == 0) && (currentTime.tm_sec < 2)) {
                _light_pattern = LIGHT_JUST_HOUR;
                break;
            } else {
                _light_pattern = LIGHT_NORMAL;
                // 分が00でない場合はdefaultでも判断させるためbreakなし。
            }

        default:
            if ((_value < 1000) && (_value % 111 == 0)) {
                _light_pattern = LIGHT_THREE_FEAVER;
            } else
            if (_value % 1111 == 0) {
                _light_pattern = LIGHT_FOUR_FEVER;
            } else {
                _light_pattern = LIGHT_NORMAL;
            }
            break;
        }
    }
};

static Calculator calc = Calculator();

#define NUMBER_OF_CHANNEL       11

static int current_channel = 0;
static struct ChannelValue channel_values[NUMBER_OF_CHANNEL];
static unsigned long last_received_at = 0;

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
    char unit[16] = { 0 };
    char buff[64] = { 0 };
    ChannelValue *channel_value;

    strncpy(buff, (const char *)data, min(data_len, 63));
    
    sscanf(buff, "%hd,%f,%s\n", &ch, &value, unit);
    if (ch < 1 || ch >= NUMBER_OF_CHANNEL) {
        Serial.printf("The channell is %d. ", ch);
        Serial.println("The channel should 1 to 10.");
        return;
    }

    channel_value = &channel_values[ch];
    channel_value->available = true;
    channel_value->value = value;
    channel_value->received_at = millis();
    strncpy(channel_value->unit, unit, 15);

    Serial.printf("<< %s\n", buff);

    // タイマーの場合は継続して表示させるためラウンデングモードにせず直ぐにチャンネルを変更する。
    bool rounding = strcmp(unit, "timer") != 0;
    if (rounding == false) {
        current_channel = ch;
    }
    set_rounding(rounding);
    last_received_at = millis();
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
    last_received_at = millis();
}

static void light_task(void *) {
    bool changed;

    light.begin();

    while (true)
    {
        switch(calc.light_pattern()) {

        case LIGHT_OFF:
            light.set_color(LColor::BLACK);
            delay(100);
            break;

        case LIGHT_NORMAL:
            light.set_color(LColor::WHITE);
            delay(100);
            break;

        case LIGHT_JUST_HOUR:
            {
                for (int i = 0; i < 3; i++) {
                    light.set_color(LColor::BLACK);
                    delay(500);
                    light.set_color(LColor::GREEN);
                    delay(500);
                }
                light.set_color(LColor::BLACK);
                delay(500);
                calc.set_light_pattern(LIGHT_NORMAL);
            }
            break;

        case LIGHT_TIMER:
            light.set_color(LColor::BLUE);
            delay(100);
            break;

        case LIGHT_LESS_ONE_MINITUE:
            light.set_color(LColor::YELLOW);
            delay(100);
            break;

        case LIGHT_LESS_THIRTY_SECONDS:
            light.set_color(LColor::BLACK);
            delay(1000);
            light.set_color(LColor::YELLOW);
            delay(1000);
            break;

        case LIGHT_LESS_TEN_SECONDS:
            light.set_color(LColor::BLACK);
            delay(500);
            light.set_color(LColor::RED);
            delay(500);
            break;

        case LIGHT_LESS_FIVE_SECONDS:
            light.set_color(LColor::BLACK);
            delay(250);
            light.set_color(LColor::RED);
            delay(250);
            break;

        case LIGHT_BANG:
            light.set_color(LColor::RED);
            delay(100);
            break;

        case LIGHT_THREE_FEAVER:
            {
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < 8; j++) {
                        light.set_color((LColor)j);
                        delay(200);
                    }
                }
                light.set_color(LColor::BLACK);
                delay(500);
                calc.set_light_pattern(LIGHT_NORMAL);
            }
            break;

        case LIGHT_FOUR_FEVER:
            {
                for (int i = 0; i < 10; i++) {
                    for (int j = 0; j < 8; j++) {
                        light.set_color((LColor)j);
                        delay(100);
                    }
                }
                light.set_color(LColor::BLACK);
                delay(500);
                calc.set_light_pattern(LIGHT_NORMAL);
            }
            break;

        default:
            delay(100);
            break;
        }
    }
}

static void display() {
    if (current_channel == 0) {
        calc.set_time(currentTime.tm_hour, currentTime.tm_min);
    } else {
        calc.set_channel_value(&channel_values[current_channel]);
    }
    calc.light_pattern();
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.update();
    // サーボが動き出すと通信ができなくなるのでAボタンを押しながら起動すると
    // サーボ初期化せず書き込みできる様にここで停止させる。
    if (M5.BtnA.isPressed()) {
        while(true) {
            delay(1000);
        }
    }

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

    xTaskCreatePinnedToCore(light_task, "light", 2048, NULL, 25, NULL, APP_CPU_NUM);
    
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    for (int i = 0; i < 4; i++)
    {
        pushers[i].begin();
    }

#if !defined(TEST_MODE) && !defined(TEST_COUNT_UP_DOWN) && !defined(TEST_LIGHT_PATTERN)
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

#ifdef TEST_LIGHT_PATTERN
static void test_light_patter() {
    for (int i = 0; i < (int)LIGHT_FOUR_FEVER + 1; i++) {
Serial.printf("pattern: %d", i);
        light_pattern = (LightPattern)i;
        delay(10000);
    }    
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
#ifdef TEST_LIGHT_PATTERN
    test_light_patter();
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

    // ラウンディングモード時はROUNDING_INTERVAL経過で次のチャンネルを表示する。
    if (rounding) {
        if (now - rounding_at >= ROUNDING_INTERVAL) {
            needs_to_change_current_channel = true;
            set_rounding(true, true);
        }
    } else {
        if (current_channel != 0) {
            // 最後の受信からROUNDING_INTERVAL経過したらラウンディングモードに戻す。
            if (now - last_received_at >= ROUNDING_INTERVAL) {
                // タイマーの場合は終了しているので無効にする。
                if (strcmp(channel_values[current_channel].unit, "timer") == 0) {
                    channel_values[current_channel].available = false;
                }
                needs_to_change_current_channel = true;
                set_rounding(true, true);
            }
        }
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
