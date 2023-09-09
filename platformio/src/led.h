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
