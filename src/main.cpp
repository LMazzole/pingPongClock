/**
    Ping Pong LED Clock
    @file     pingPongClock.ino
    @author   Yiwei Mao
    @version  1.0.0
    @email    ewaymao@gmail.com
    @github   https://github.com/YiweiMao
    @twitter  https://twitter.com/ewaymao
    @blog     https://yiweimao.github.io/blog/

    Whole software fits on an Arduino (ATmega328P) Nano. Sketch compiles (without scrolling text support) to:
    - Sketch uses 10236 bytes (33%) of program storage space. Maximum is 30720 bytes.
    - Global variables use 1807 bytes (88%) of dynamic memory, leaving 241 bytes for local variables. Maximum is 2048 bytes.

    Build instructions from https://www.instructables.com/Ping-Pong-Ball-LED-Clock/
    The following foreground and background modes can be mixed and matched!

    Foreground Modes: mode_fg
    - 'T': Single colour time mode
    - 'R': Scrolling rainbow time mode
    - 'N': No time
    - 'C': Cycle through all digits 0--9999 quickly
    - is_slanted: Option to use slanted digits or original digits (from https://www.instructables.com/Ping-Pong-Ball-LED-Clock/)

    Background Animation Modes: mode_bg
    - 'R': Scrolling rainbow background
    - 'B': No background
    - 'T': Twinkle
    - 'F': Fireworks
    - 'W': Thunderstorm
    - 'H': Firepit (works well with single colour time mode set to a light teal)

    Improvements:
    - Use a hardware RTC rather than use software
    - Implement scolling text
    - Use FastLED colour palettes
    - Attach light sensor and auto-adjust FastLED brightness
    - Attach PIR motion sensor and turn on display when there is someone to look at it
    - Attach temperature/humidity/pressure sensor and display stats
    - Connect to Wifi (e.g. using an ESP32)
*/
#include <Arduino.h>
#include <FastLED.h>
#include <RTClib.h>  // Adafruit RTClib

// IO-MAPPING
#ifdef BUILD_FOR_NANO
const int LED_PIN = 6;
#elif BUILD_FOR_ESP32
const int LED_PIN = 23;
#endif

/** CONFIGURATION **/
const int NUM_LEDS = 128;  // Nbr of LEDS's
const int REFRESH_RATE_HZ = 20;
const int FRAME_TIME_MS = (1000 / REFRESH_RATE_HZ);

// Settings
// char mode_fg = 'R';  // 'T' time, 'R' rainbow time, 'N' no op (time doesn't show), 'C' cycle through all digits

enum class ModeFG { None,         // 'N' no op (time doesn't show)
                    Time,         // 'T' time
                    TimeRainbow,  // 'R' rainbow time,
                    Cycle         // 'C' cycle through all digits
};

enum class ModeBG { None,              // 'B': No background
                    SolidColor,        // 'S': One color
                    ScrollingRainbow,  // 'R': Scrolling rainbow background
                    Twinkle,           // 'T': Twinkle
                    Fireworks,         // 'F': Fireworks
                    Thunderstorm,      // 'W': Thunderstorm
                    Firepit            // 'H': Firepit (works well with single colour time mode set to a light teal)
};

struct Foreground {
    ModeFG Mode = ModeFG::Time;
    CRGB DefaultColour = CRGB::Snow;
    bool is_slant = false;  // Display digits as slanted
};

struct Background {
    ModeBG Mode = ModeBG::SolidColor;
    CRGB DefaultColour = CRGB::DarkBlue;
};

struct Led {
    Foreground Fg;
    Background Bg;
} LedDisplay;

// LEDs

/**
 * Imagining the display as a parallelogram slanted to the left,
 * I turned Figure 9 into a two dimensional array (look up table) with values corresponding to the strip index.
 * For the positions that don't exist, I put values of 999.
 *
 *        / 012 013 ...
 *      / 001 011   ...
 *    / 002 010 015 ...
 *  < 000 003 009   ...
 *    \ 004 008 017 ...
 *      \ 005 007   ...
 *        \ 006 019 ...
 *
 * */
const int led_address[7][20] = {
    {999, 999, 999, 12, 13, 26, 27, 40, 41, 54, 55, 68, 69, 82, 83, 96, 97, 110, 111, 124},  // 0th row
    {999, 999, 1, 11, 14, 25, 28, 39, 42, 53, 56, 67, 70, 81, 84, 95, 98, 109, 112, 123},    // 1st row
    {999, 2, 10, 15, 24, 29, 38, 43, 52, 57, 66, 71, 80, 85, 94, 99, 108, 113, 122, 125},    // 2nd row
    {0, 3, 9, 16, 23, 30, 37, 44, 51, 58, 65, 72, 79, 86, 93, 100, 107, 114, 121, 126},      // 3rd row
    {4, 8, 17, 22, 31, 36, 45, 50, 59, 64, 73, 78, 87, 92, 101, 106, 115, 120, 127, 999},    // 4th row
    {5, 7, 18, 21, 32, 35, 46, 49, 60, 63, 74, 77, 88, 91, 102, 105, 116, 119, 999, 999},    // 5th row
    {6, 19, 20, 33, 34, 47, 48, 61, 62, 75, 76, 89, 90, 103, 104, 117, 118, 999, 999, 999},  // 6th row
};

RTC_Millis rtc;  // Time keeping
CRGB leds[NUM_LEDS];
DateTime now;  // time record

/** FOREGROUND **/

/** DIGITS **/
// Look up tables for how to build alphanumeric characters
// referenced from leftmost
const int digits[10][10] = {
    {7, 8, 10, 11, 14, 18, 22, 24},         // 0
    {14, 15, 16, 17, 18},                   // 1
    {7, 8, 9, 11, 14, 16, 18, 24},          // 2
    {7, 9, 11, 14, 16, 18, 22, 24},         // 3
    {9, 10, 11, 16, 18, 22, 24},            // 4
    {7, 9, 10, 11, 14, 16, 18, 22},         // 5
    {7, 8, 9, 14, 15, 16, 18, 22},          // 6
    {7, 11, 14, 16, 17, 24},                // 7
    {7, 8, 9, 10, 11, 14, 16, 18, 22, 24},  // 8
    {7, 9, 10, 11, 14, 16, 17, 24},         // 9
};
const int digits_len[10] = {8, 5, 8, 8, 7, 8, 8, 6, 10, 8};

/** SLANTED DIGITS **/

// referenced from one place to the right because not all digits will fit at leftmost
const int slant_digits[10][13] = {
    {39, 42, 53, 52, 44, 45, 35, 32, 21, 31, 30, 38},      // 0
    {35, 45, 44, 52, 53},                                  // 1
    {39, 42, 53, 52, 44, 37, 30, 31, 21, 32, 35},          // 2
    {39, 42, 53, 52, 44, 37, 30, 45, 35, 32, 21},          // 3
    {39, 38, 30, 37, 44, 52, 53, 45, 35},                  // 4
    {53, 42, 39, 38, 30, 37, 44, 45, 35, 32, 21},          // 5
    {53, 42, 39, 38, 30, 37, 44, 45, 35, 32, 21, 31},      // 6
    {39, 42, 53, 52, 44, 45, 35, 38},                      // 7
    {53, 42, 39, 38, 30, 37, 44, 45, 35, 32, 21, 31, 52},  // 8
    {53, 42, 39, 38, 30, 37, 44, 45, 35, 32, 21, 52},      // 9
};
const int slant_digits_len[10] = {12, 5, 11, 11, 9, 11, 12, 8, 13, 12};

int cycle_counter = 0;  // for displaying all digits quickly 0--9999

/**
 * @brief
 * @param int hour
 * @param int min
 * @param int sec
 * @param char Foreground Modes
 * @param bool Slanted Digits
 **/
void disp_time(int hour, int min, int sec, Foreground &fg);

/**
 * @brief
 * @param int num
 * @param int offset
 * @param bool Slanted Digits
 **/
void disp_num(int num, int offset, Foreground &fg);

/**
 * @brief Set color for foreground
 * @param int index
 * @param Foreground Struct containging foreground settings
 * @return CRGB
 **/
CRGB fg_palette(int indx, Foreground &fg);

/*
// TEXT
// A--Z ! . : ^
const int slant_chars[30][13] = {
  {7},
};
const int slant_chars_len[30] = {1};

void disp_str(char *str) {
};
*/

/** BACKGROUND **/
// char bg_palette = 'B';  // 'R' rainbow, 'B' black, 'T' twinkle, 'F' fireworks, 'W' rain, 'H' firepit
CHSV bg_colour(64, 255, 190);
int bg_counter = 0;

/**
 * @brief Display background in one solid color
 **/
void bg_solidColor(Background &bg);

/** RAINBOW **/
/**
 * @brief
 **/
void bg_rainbow();

/** TWINKLE **/
const int MAX_TWINKLES = 8;
struct twinkle_t {
    int pos = -1;   // LED position 0--127
    int stage = 0;  // record of how bright each twinkle is up to. 0--16
};
struct twinkle_t twinkles[MAX_TWINKLES];

/**
 * @brief
 **/
void bg_twinkle();

/** RAIN **/
const int MAX_RAINDROPS = 16;
struct rain_t {
    int pos = -1;  // first row position
    int stage = 0;
    bool lightning = false;  // 0 normal rain, 1 is ligtning
    int prev_pos[6];         // holds lightning positions to clear later
};
struct rain_t raindrops[MAX_RAINDROPS];

/**
 * @brief
 **/
void bg_rain();

/** FIREWORK **/
const int MAX_FIREWORKS = 5;

struct firework_t {
    int pos = -1;           // LED number in last row
    int direction = 0;      // 0 is left, 1 is right
    int stage = 0;          // remember where each firework animation is up to
    char hue = 0;           // colour of each firework
    int height_offset = 0;  // sometimes lower by one.
};
struct firework_t fireworks[MAX_FIREWORKS];

/**
 * @brief
 **/
void bg_firework();

/**
 * @brief
 **/
void bg_firepit();

/** Update Functions **/
/**
 * @brief
 **/
void update_LEDs(Led &LedDisplay);

/**
 * The setup method used by the Arduino.
 */
void setup() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    // limit my draw to 1A at 5v of power draw
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
    // FastLED.setBrightness(  BRIGHTNESS );
    FastLED.clear();
    FastLED.show();

    // This line sets the RTC with an explicit date & time, for example to set
    // January 5, 2021 at 1:37pm you would call:
    // rtc.adjust(DateTime(2021, 1, 05, 13, 37, 0));
    // Note: F() grabs constants from program memory (flash) rather than RAM
    rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
}

/**
 * The main runloop used by the Arduino.
 */
void loop() {
    update_LEDs(LedDisplay);
    FastLED.delay(FRAME_TIME_MS);
}

// Functions

void update_LEDs(Led &LedDisplay) {
    // update the background
    switch (LedDisplay.Bg.Mode) {
        case ModeBG::None:
            FastLED.clear();
            break;
        case ModeBG::SolidColor:
            bg_solidColor(LedDisplay.Bg);
            break;
        case ModeBG::ScrollingRainbow:
            bg_rainbow();
            break;
        case ModeBG::Twinkle:
            FastLED.clear();
            bg_twinkle();
            break;
        case ModeBG::Fireworks:
            FastLED.clear();
            bg_firework();
            break;
        case ModeBG::Thunderstorm:
            FastLED.clear();
            bg_rain();
            break;
        case ModeBG::Firepit:
            FastLED.clear();
            bg_firepit();
            break;
        default:
            break;
    }

    // update the foreground
    switch (LedDisplay.Fg.Mode) {
        case ModeFG::Time:
        case ModeFG::TimeRainbow:
            now = rtc.now();
            disp_time(now.hour(), now.minute(), now.second(), LedDisplay.Fg);
            FastLED.show();
            break;
        case ModeFG::None:  // No operation
            FastLED.show();
            break;
        case ModeFG::Cycle:
            disp_time(cycle_counter / 100, cycle_counter % 100, 0, LedDisplay.Fg);
            cycle_counter++;
            if (cycle_counter == 10000)
                cycle_counter = 0;
            FastLED.show();
            break;
        default:
            FastLED.clear();
            break;
    }
}

/** ================ FOREGROUND ================ **/

void disp_time(int hour, int min, int sec, Foreground &fg) {
    if ((fg.Mode == ModeFG::TimeRainbow) || (fg.Mode == ModeFG::Cycle)) {
        if (bg_counter < REFRESH_RATE_HZ / 4)
            bg_counter++;
        else {
            bg_colour.hue = (bg_colour.hue + 1) % 256;
            bg_counter = 0;
        }
    }

    // Write Digits
    disp_num(hour / 10, 0, fg);       // 1. Digit 10Hours
    disp_num(hour % 10, 28, fg);      // 2. Digit 1Hour
    disp_num(min / 10, 70, fg);       // 3. Digit 10 Min
    disp_num(min % 10, 70 + 28, fg);  // 4. Digit 1Min

    // seconds tick between Digti 2 and 3 refreshed all 2 seconds
    if (sec % 2 == 0) {
        // Upper dot
        leds[66] = fg_palette(66, fg);

        // Lower dot
        if (fg.is_slant) {
            leds[59] = fg_palette(59, fg);
        } else {
            leds[64] = fg_palette(64, fg);
        }
    }
}

void disp_num(int num, int offset, Foreground &fg) {
    if (fg.is_slant) {
        for (int i = 0; i < slant_digits_len[num]; i++) {
            int indx = slant_digits[num][i] + offset - 28;
            if (indx < 7)
                indx++;  // adjust when LEDS really close to the start of the strip
            if (indx >= 0 && indx < 128)
                leds[indx] = fg_palette(indx, fg);
        }
    } else {
        for (int i = 0; i < digits_len[num]; i++) {
            leds[digits[num][i] + offset] = fg_palette(digits[num][i] + offset, fg);
        }
    }
}

CRGB fg_palette(int indx, Foreground &fg) {
    // Check if index is valid
    if (indx < 0 && indx >= NUM_LEDS) {
        return CRGB::Black;
    }
    // Mode Scrolling Rainbow Time or Cyclic
    if ((fg.Mode == ModeFG::TimeRainbow) || (fg.Mode == ModeFG::Cycle)) {
        return CHSV((bg_colour.hue + indx) % 256, bg_colour.sat, bg_colour.val);
    }

    return fg.DefaultColour;
}

/** ================ BACKGROUND ================ **/
void bg_solidColor(Background &bg) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = bg.DefaultColour;
    }
}
void bg_rainbow() {
    if (bg_counter < REFRESH_RATE_HZ / 4)
        bg_counter++;
    else {
        bg_colour.hue = (bg_colour.hue + 1) % 256;
        bg_counter = 0;
    }

    // show half the hues
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV((bg_colour.hue + i) % 256, bg_colour.sat, bg_colour.val);
    }
}

void bg_twinkle() {
    int empty_slot = -1;
    for (int i = 0; i < MAX_TWINKLES; i++) {
        if (twinkles[i].pos == -1) {
            empty_slot = i;
            break;
        }
    }
    if (random8() < 96 && empty_slot != -1) {
        twinkles[empty_slot].pos = random(NUM_LEDS);
        twinkles[empty_slot].stage = 16;
    }

    for (int i = 0; i < MAX_TWINKLES; i++) {
        if (twinkles[i].pos != -1 && twinkles[i].stage > 0) {
            int brightness = 8 * twinkles[i].stage;
            leds[twinkles[i].pos] = CRGB(brightness, brightness, brightness);  // set to white/gray
            twinkles[i].stage--;
            if (twinkles[i].stage == 0)
                twinkles[i].pos = -1;
        }
    }
}

void bg_rain() {
    int empty_slot = -1;
    // Set background
    for (int i = 3; i < 20; i++) {
        leds[led_address[0][i]] = CRGB::Gray;
    }
    for (int i = 2; i < 20; i++) {
        leds[led_address[1][i]] = CHSV(0, 0, random8(64, 128));
    }

    for (int i = 0; i < MAX_RAINDROPS; i++) {
        if (raindrops[i].pos == -1) {
            empty_slot = i;
            break;
        }
    }

    if (random8() < 200 && empty_slot != -1) {
        raindrops[empty_slot].pos = random8(3, 21);  // 3--20
        raindrops[empty_slot].stage = 1;
        raindrops[empty_slot].lightning = random8(0, 20) / 19;          // 0--1 with 1 happening ~5%
        raindrops[empty_slot].prev_pos[0] = raindrops[empty_slot].pos;  // remember the path the raindrop takes
    }

    for (int i = 0; i < MAX_RAINDROPS; i++) {
        if (raindrops[i].pos != -1 && raindrops[i].stage > 0) {
            if (raindrops[i].lightning != 0 && raindrops[i].stage == 1) {
                int x = raindrops[i].pos;
                for (int j = 1; j <= 6; j++) {
                    x -= random8(0, 2);
                    x = (x >= 0 && x < 20) ? x : 0;
                    int indx = led_address[j][x];
                    if (indx >= 0 && indx < NUM_LEDS) {
                        leds[indx] = CRGB::Yellow;
                        raindrops[i].prev_pos[j - 1] = indx;
                    }
                }
            } else if (raindrops[i].lightning != 0 && raindrops[i].stage > 1 && raindrops[i].stage < 7) {
                for (int j = 0; j < 6; j++)
                    leds[raindrops[i].prev_pos[j]] = CRGB::Yellow;
            } else {  // rain
                int x = raindrops[i].prev_pos[raindrops[i].stage - 1] - random8(0, 2);
                x = (x >= 0 && x < 20) ? x : 0;
                raindrops[i].prev_pos[raindrops[i].stage] = x;
                int indx = led_address[raindrops[i].stage][x];
                if (indx >= 0 && indx < NUM_LEDS)
                    leds[indx] = CHSV(HUE_BLUE, 255, 128);
                else
                    raindrops[i].stage = 6;
            }

            raindrops[i].stage++;
            if (raindrops[i].stage == 7 && raindrops[i].lightning != 0) {
                raindrops[i].pos = -1;
                for (int j = 0; j < 6; j++)
                    leds[raindrops[i].prev_pos[j]] = CRGB::Black;
            } else if (raindrops[i].stage == 7 && raindrops[i].lightning == 0)
                raindrops[i].pos = -1;
        }
    }
}

void bg_firework() {
    const int START_STAGE = 24;  //    Starting stage
    int empty_slot = -1;
    for (int i = 0; i < MAX_FIREWORKS; i++) {
        if (fireworks[i].pos == -1) {
            empty_slot = i;
            break;
        }
    }

    if (random8() < 24 && empty_slot != -1) {
        fireworks[empty_slot].pos = random8(3, 14);  // 3--13
        fireworks[empty_slot].stage = START_STAGE;
        fireworks[empty_slot].direction = random8(0, 2);      // 0--1
        fireworks[empty_slot].hue = random8();                // 0--255
        fireworks[empty_slot].height_offset = random8(0, 2);  // 0--1
    }

    for (int i = 0; i < MAX_FIREWORKS; i++) {
        if (fireworks[i].pos != -1 && fireworks[i].stage > 0) {
            // final position of firework explosion
            int y = 2 + fireworks[i].height_offset;
            int x = fireworks[i].pos + 4 * fireworks[i].direction;

            if (fireworks[i].stage == START_STAGE)
                // Set startpoint to white
                leds[led_address[6][fireworks[i].pos]] = CRGB::White;
            else if (fireworks[i].stage >= (20 + fireworks[i].height_offset)) {
                int level = 6 - (24 - fireworks[i].stage);
                leds[led_address[level][fireworks[i].pos + (6 - level) * fireworks[i].direction]] = CRGB::White;
                leds[led_address[level + 1][fireworks[i].pos + (6 - level + 1) * fireworks[i].direction]] = CRGB::Black;
            } else if ((fireworks[i].stage == 18) || (fireworks[i].stage == 17)) {
                // explode in 6 directions from (x,y)
                leds[led_address[y][x]] = CRGB::Black;
                leds[led_address[y - 1][x + 1]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y][x + 1]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y + 1][x]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y + 1][x - 1]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y][x - 1]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y - 1][x]] = CHSV(fireworks[i].hue, 255, 255);
            } else if (fireworks[i].stage == 16) {
                // explode in 6 directions from (x,y)
                leds[led_address[y][x]] = CRGB::Black;
                leds[led_address[y - 1][x + 1]] = CRGB::Black;
                leds[led_address[y][x + 1]] = CRGB::Black;
                leds[led_address[y + 1][x]] = CRGB::Black;
                leds[led_address[y + 1][x - 1]] = CRGB::Black;
                leds[led_address[y][x - 1]] = CRGB::Black;
                leds[led_address[y - 1][x]] = CRGB::Black;

                leds[led_address[y - 2][x + 2]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y][x + 2]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y + 2][x]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y + 2][x - 2]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y][x - 2]] = CHSV(fireworks[i].hue, 255, 255);
                leds[led_address[y - 2][x]] = CHSV(fireworks[i].hue, 255, 255);
            } else if (fireworks[i].stage > 0) {
                // explode in 6 directions from (x,y) and fade
                int brightness = 16 * fireworks[i].stage;
                leds[led_address[y - 2][x + 2]] = CHSV(fireworks[i].hue, 255, brightness);
                leds[led_address[y][x + 2]] = CHSV(fireworks[i].hue, 255, brightness);
                leds[led_address[y + 2][x]] = CHSV(fireworks[i].hue, 255, brightness);
                leds[led_address[y + 2][x - 2]] = CHSV(fireworks[i].hue, 255, brightness);
                leds[led_address[y][x - 2]] = CHSV(fireworks[i].hue, 255, brightness);
                leds[led_address[y - 2][x]] = CHSV(fireworks[i].hue, 255, brightness);
            }

            fireworks[i].stage--;
            if (fireworks[i].stage == 0)
                fireworks[i].pos = -1;
        }
    }
}

void bg_firepit() {
    for (int level = 6; level > 2; level--) {
        for (int i = 0; i < 17 + (6 - level); i++) {
            leds[led_address[level][i]] = CHSV(HUE_RED + random8(8), 255, random8(192 - (6 - level) * 64, 255 - (6 - level) * 64));
        }
    }
}