/**
 * @file PLedDisp.h
 * @brief
 *
 * Build instructions from https://www.instructables.com/Ping-Pong-Ball-LED-Clock/
 * The following foreground and background modes can be mixed and matched!
 * @version 2.0 - Refactored into class - Luca Mazzoleni
 * @version 1.0 - Original Version - Yiwei Mao
 * @github   https://github.com/YiweiMao
 * @twitter  https://twitter.com/ewaymao
 * @blog     https://yiweimao.github.io/blog/
 */

#pragma once

#include <FastLED.h>
#include <RTClib.h>  // Adafruit RTClib

// IO-MAPPING
#ifdef BUILD_FOR_NANO
const int LED_PIN = 6;
#elif BUILD_FOR_ESP32
const int LED_PIN = 0;
#endif
const int NUM_LEDS = 128;  // Nbr of LEDS's in Display

const int MAX_TWINKLES = 8;
const int MAX_RAINDROPS = 16;
const int MAX_FIREWORKS = 5;
extern RTC_Millis RTC_TIME;
extern DateTime TIME_NOW;

class PLedDisp {
    //=====PUBLIC====================================================================================
   public:
    /**
     * @brief Foreground modes
     */
    enum class ModeFG { None,         // no op (time doesn't show)
                        Time,         // time
                        TimeRainbow,  // rainbow time,
                        Cycle         // cycle through all digits 0--9999 quickly
    };

    /**
     * @brief Background modes
     */
    enum class ModeBG { None,              // No background
                        SolidColor,        // One color
                        ScrollingRainbow,  // Scrolling rainbow background
                        Twinkle,           // Twinkle
                        Fireworks,         // Fireworks
                        Thunderstorm,      // Thunderstorm
                        Firepit            // Firepit (works well with single colour time mode set to a light teal)
    };

    /**
     * @brief Frame modes
     */
    enum class ModeFR { None,        // No background
                        SolidColor,  // One color
                        Time         // Like a seconds display
    };

    /**
     * @brief Construct a new PLedDisp object
     *
     */
    PLedDisp();

    /**
     * @brief Destroy the PLedDisp object
     *
     */
    ~PLedDisp();

    /**
     * @brief Set the Background Mode
     *
     * @param mode - Background mode to set e.g. ModeBG::Firepit
     */
    void setBackgroundMode(ModeBG mode);

    /**
     * @brief Set the Background Color object when Mode solidColor is active
     *
     * @param color - Backgroundcolor e.g. CRGB::Red
     */
    void setBackgroundColor(CRGB color);

    /**
     * @brief Set the Frame Mode object
     *
     * @param mode - Frame mode to set e.g ModeFR::Time
     */
    void setFrameMode(ModeFR mode);

    /**
     * @brief Set the Frame Color object when Mode solidColor is active
     *
     * @param color - Framecolor e.g. CRGB::Red
     */
    void setFrameColor(CRGB color);

    /**
     * @brief Set the Foreground Mode object
     *
     * @param mode - Frame mode to set e.g ModeFG::Time
     * @param TextSlanted - Default false. Set true if text should be displayed italic/slanted.
     */
    void setForegroundMode(ModeFG mode, bool TextSlanted = false);

    /**
     * @brief Set the Foreground Color object
     *
     * @param color - Foreground e.g. CRGB::Red
     */
    void setForegroundColor(CRGB color);

    /**
     * @brief Set the Warnings indicator active
     *
     * @param indicator - 0-4 . Warining-Leds bottom left
     * @param statusOk - False generates a warning
     * @param Level - Severity level 1 = Warning, 2 = Error, 0 = disabled
     */
    void setWarning(uint indicator, bool statusOk, uint Level = 1);

    /**
     * @brief Updateds PingpongLed display.
     * Changes are only visible when this function is called
     */
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
    struct Foreground {
        ModeFG Mode = ModeFG::Time;
        CRGB Color = CRGB::Peru;
        bool is_slant = true;  // Display digits as slanted
    } Fg;
    struct Background {
        ModeBG Mode = ModeBG::SolidColor;
        CRGB Color = CRGB::Black;
    } Bg;
    struct Frame {
        ModeFR Mode = ModeFR::None;
        CRGB Color = CRGB::DarkGrey;
    } Fr;

    CRGB leds[NUM_LEDS];  // Define the array of leds
    // DateTime now;         // time record
    CHSV bg_colour;
    int ErrorIndicator[4] = {};
    const int ErrorIndicatorAdr[4] = {118, 119, 127, 126};
    const int REFRESH_RATE_HZ = 50;  // Refrasherate of LED's and animation
    const int FRAME_TIME_MS = (1000.0 / REFRESH_RATE_HZ);
    unsigned long currentMillis = 0;   ///< Current time for non blocking delay
    unsigned long previousMillis = 0;  ///< Last time called for non blocking delay

    int cycle_counter = 0;  // for displaying all digits quickly 0--9999
    int bg_counter = 0;

    struct twinkle_t {
        int pos = -1;   // LED position 0--127
        int stage = 0;  // record of how bright each twinkle is up to. 0--16
    } twinkles[MAX_TWINKLES];
    struct rain_t {
        int pos = -1;  // first row position
        int stage = 0;
        bool lightning = false;  // 0 normal rain, 1 is ligtning
        int prev_pos[6];         // holds lightning positions to clear later
    } raindrops[MAX_RAINDROPS];
    struct firework_t {
        int pos = -1;           // LED number in last row
        int direction = 0;      // 0 is left, 1 is right
        int stage = 0;          // remember where each firework animation is up to
        char hue = 0;           // colour of each firework
        int height_offset = 0;  // sometimes lower by one.
    } fireworks[MAX_FIREWORKS];

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

    const int frame[44] = {68, 69, 82, 83, 96, 97, 110, 111, 124,
                           123, 125, 126, 127, 119,
                           118, 117, 104, 103, 90, 89, 76, 75, 62, 61, 48, 47, 34, 33, 20, 19, 6,
                           5, 4, 0, 2, 1,
                           12, 13, 26, 27, 40, 41, 54, 55};

    /**
     * @brief Display time in foreground
     *
     * @param time - Time to display
     * @param fg - Foregroundsettings
     */
    void disp_time(DateTime &time, Foreground &fg);

    /**
     * @brief Display 4 digits in foreground
     *
     * @param Digit3 - 1000*
     * @param Digit2 - 100*
     * @param Digit1 - 10*
     * @param Digit0 - 1* = 0-9
     * @param fg - Foregroundsettings
     */
    void disp_number(uint8_t Digit3, uint8_t Digit2, uint8_t Digit1, uint8_t Digit0, Foreground &fg);

    /**
     * @brief Display a digit
     *
     * @param num - Number to display
     * @param offset - Offset to first LED
     * @param fg - Foregroundsettings
     */
    void disp_digit(int num, int offset, Foreground &fg);

    /**
     * @brief Get color for LED on this index
     *
     * @param indx - Address of LED
     * @param fg - Foregroundsettings
     * @return CRGB - Color to return
     */
    CRGB fg_palette(int indx, Foreground &fg);

    /**
     * @brief Display frame as solod color
     *
     * @param fr - Framesettings containg color
     */
    void fr_solidColor(Frame &fr);

    /**
     * @brief Display frame as a second hand
     *
     * @param time - actual time to display
     * @param fr - Framesettings containg color
     */
    void fr_time(DateTime &time, Frame &fr);

    /**
     * @brief Display background in one solid color
     *
     * @param bg - Backgroundsettings containing color
     */
    void bg_solidColor(Background &bg);

    /**
     * @brief Display background as rainbow color
     **/
    void bg_rainbow();

    /**
     * @brief Display background with twinkles
     **/
    void bg_twinkle();

    /**
     * @brief Display background as rain
     **/
    void bg_rain();

    /**
     * @brief Display background with fireworks
     **/
    void bg_firework();

    /**
     * @brief Display background as firepit
     **/
    void bg_firepit();
};
