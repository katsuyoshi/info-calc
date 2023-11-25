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

#ifndef _PUSHER_H_
#define _PUSHER_H_

// for pusher
typedef enum
{
    ServoStateInit,
    ServoStateOffA,
    ServoStateA,
    ServoStateOffB,
    ServoStateB,
} ServoState;

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

#endif
