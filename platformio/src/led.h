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

#ifndef _LED_H_
#define _LED_H_

char led_clock_pattern[] = 
    " BRB "
    "B R B"
    "B RRB"
    "B   B"
    " BBB ";

// over 66 percent
char led_hot_temperature[] = 
    "R RRR"
    " R   "
    " R   "
    " R   "
    "  RRR";

// 33 to 66 percent
char led_norm_temperature[] = 
    "G GGG"
    " G   "
    " G   "
    " G   "
    "  GGG";

// under 33 percent
char led_cold_temperature[] = 
    "B BBB"
    " B   "
    " B   "
    " B   "
    "  BBB";

char led_high_humidity[] = 
    "R   R"
    "   R "
    "  R  "
    " R   "
    "R   R";

char led_norm_humidity[] = 
    "G   G"
    "   G "
    "  G  "
    " G   "
    "G   G";

char led_low_humidity[] = 
    "B   B"
    "   B "
    "  B  "
    " B   "
    "B   B";

#endif
