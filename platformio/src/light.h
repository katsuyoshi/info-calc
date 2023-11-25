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

#ifndef _LIGHT_H_
#define _LIGHT_H_

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

#endif
