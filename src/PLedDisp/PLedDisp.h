/**
 * @file PLedDisp.h
 * @brief
 *
 * @author Luca Mazzoleni (luca_mazzoleni92@hotmail.com)
 *
 * @version 1.0 - Description - {author} - {date}
 *
 * @date 2022-01-05
 * @copyright Copyright (c) 2022
 *
 */

#ifndef PLEDDISP_H__
#define PLEDDISP_H__

#include <FastLED.h>
#include <RTClib.h>  // Adafruit RTClib

// IO-MAPPING
#ifdef BUILD_FOR_NANO
const int LED_PIN = 6;
#elif BUILD_FOR_ESP32
const int LED_PIN = 23;
#endif
const int NUM_LEDS = 128;  // Nbr of LEDS's

const int MAX_TWINKLES = 8;
const int MAX_RAINDROPS = 16;
const int MAX_FIREWORKS = 5;

class PLedDisp {
    //=====PUBLIC====================================================================================
   public:
    enum class ModeFG { None,         // 'N' no op (time doesn't show)
                        Time,         // 'T' time
                        TimeRainbow,  // 'R' rainbow time,
                        Cycle         // 'C' cycle through all digits
    };

    enum class ModeBG { None,              // 'N': No background
                        SolidColor,        // 'S': One color
                        ScrollingRainbow,  // 'R': Scrolling rainbow background
                        Twinkle,           // 'T': Twinkle
                        Fireworks,         // 'F': Fireworks
                        Thunderstorm,      // 'W': Thunderstorm
                        Firepit            // 'H': Firepit (works well with single colour time mode set to a light teal)
    };

    enum class ModeFR { None,        // No background
                        SolidColor,  // One color
                        Time         // Like a seconds display
    };

    PLedDisp(/* args */);
    ~PLedDisp();
    void setBackgroundMode(ModeBG mode);
    void setBackgroundColor(CRGB color);
    void setFrameMode(ModeFR mode);
    void setFrameColor(CRGB color);
    void setForegroundMode(ModeFG mode, bool TextSlanted = false);
    void setForegroundColor(CRGB color);
    void update_LEDs();

    /**
     * @brief Set the Brightness object
     *
     * @param scale - a 0-255 value for how much to scale all leds before writing them out
     */
    inline void setBrightness(uint8_t scale = 255) {
        FastLED.setBrightness(scale);
    }

    //=====PRIVATE====================================================================================
   private:
    RTC_Millis rtc;       // Time keeping
    CRGB leds[NUM_LEDS];  // Define the array of leds
    DateTime now;         // time record
    CHSV bg_colour;
    const int REFRESH_RATE_HZ = 20;
    const int FRAME_TIME_MS = (1000 / REFRESH_RATE_HZ);
    unsigned long currentMillis = 0;   ///< will store current time for non blocking delay
    unsigned long previousMillis = 0;  ///< will store last time called for non blocking delay

    struct Foreground {
        ModeFG Mode = ModeFG::Time;
        CRGB Color = CRGB::Snow;
        bool is_slant = false;  // Display digits as slanted
    };
    struct Background {
        ModeBG Mode = ModeBG::SolidColor;
        CRGB Color = CRGB::DarkBlue;
    };
    struct Frame {
        ModeFR Mode = ModeFR::None;
        CRGB Color = CRGB::DarkOrange;
    };
    struct Led {
        Foreground Fg;  // Foreground
        Frame Fr;       // Frame
        Background Bg;  // Background
    } LedDisplay;
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

    /** FOREGROUND **/

    /** DIGITS **/
    // Look up tables for how to build alphanumeric characters
    // referenced from leftmost
    // const int digitsMatrix[10][10][2] = {
    //     {{5, 1}, {4, 1}, {2, 2}, {1, 3}, {1, 4}, {5, 2}, {4, 3}, {2, 4}},                  // 0
    //     {{1, 4}, {2, 3}, {3, 3}, {4, 2}, {5, 2}},                                          // 1
    //     {{5, 1}, {4, 1}, {3, 2}, {1, 3}, {1, 4}, {3, 3}, {5, 2}, {2, 4}},                  // 2
    //     {{5, 1}, {3, 2}, {1, 3}, {1, 4}, {3, 3}, {5, 2}, {4, 3}, {2, 4}},                  // 3
    //     {{3, 2}, {2, 2}, {1, 3}, {3, 3}, {5, 2}, {4, 3}, {2, 4}},                          // 4
    //     {{5, 1}, {3, 2}, {2, 2}, {1, 3}, {1, 4}, {3, 3}, {5, 2}, {4, 3}},                  // 5
    //     {{5, 1}, {4, 1}, {3, 2}, {1, 4}, {2, 3}, {3, 3}, {5, 2}, {4, 3}},                  // 6
    //     {{5, 1}, {1, 3}, {1, 4}, {3, 3}, {4, 2}, {2, 4}},                                  // 7
    //     {{5, 1}, {4, 1}, {3, 2}, {2, 2}, {1, 3}, {1, 4}, {3, 3}, {5, 2}, {4, 3}, {2, 4}},  // 8
    //     {{5, 1}, {3, 2}, {2, 2}, {1, 3}, {1, 4}, {3, 3}, {4, 2}, {2, 4}},                  // 9
    // };
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

    const int frame[44] = {68, 69, 82, 83, 96, 97, 110, 111, 124,
                           123, 125, 126, 127, 119,
                           118, 117, 104, 103, 90, 89, 76, 75, 62, 61, 48, 47, 34, 33, 20, 19, 6,
                           5, 4, 0, 2, 1,
                           12, 13, 26, 27, 40, 41, 54, 55};
    // FOREGROUND

    void disp_time(DateTime &time, Foreground &fg);

    void disp_number(uint8_t Digit3, uint8_t Digit2, uint8_t Digit1, uint8_t Digit0, Foreground &fg);

    /**
     * @brief
     * @param int num
     * @param int offset
     * @param bool Slanted Digits
     **/
    void disp_digit(int num, int offset, Foreground &fg);

    /**
     * @brief Set color for foreground
     * @param int index
     * @param Foreground Struct containging foreground settings
     * @return CRGB
     **/
    CRGB fg_palette(int indx, Foreground &fg);

    int bg_counter = 0;

    void fr_solidColor(Frame &fr);

    void fr_time(DateTime &time, Frame &fr);

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
};

#endif