/**
 * @file pLedDisp.cpp
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

#include "PLedDisp.h"
//=====PUBLIC====================================================================================
PLedDisp::PLedDisp() {
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    // limit my draw to 8A at 5v of power draw
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
    // FastLED.setBrightness(  BRIGHTNESS );
    FastLED.clear();
    FastLED.show();
    FastLED.setMaxRefreshRate(REFRESH_RATE_HZ);
    FastLED.setBrightness(100);
    CHSV bg_colour(64, 255, 190);
}

PLedDisp::~PLedDisp() {
}

void PLedDisp::setBackgroundMode(ModeBG mode) {
    this->Bg.Mode = mode;
}
void PLedDisp::setBackgroundColor(CRGB color) {
    this->Bg.Color = color;
}

void PLedDisp::setFrameMode(ModeFR mode) {
    this->Fr.Mode = mode;
}

void PLedDisp::setFrameColor(CRGB color) {
    this->Fr.Color = color;
}

void PLedDisp::setForegroundMode(ModeFG mode, bool TextSlanted) {
    this->Fg.is_slant = TextSlanted;
    this->Fg.Mode = mode;
}
void PLedDisp::setForegroundColor(CRGB color) {
    this->Fg.Color = color;
}

void PLedDisp::setWarning(uint indicator, bool statusOk, uint Level) {
    if (indicator < sizeof(ErrorIndicator) / sizeof(ErrorIndicator[0])) {
        ErrorIndicator[indicator] = ((statusOk == false) * Level);
    }
}

void PLedDisp::update_LEDs() {
    currentMillis = millis();
    if ((currentMillis - previousMillis) > FRAME_TIME_MS) {
        previousMillis = currentMillis;
    } else {
        return;
    }

    // update the background
    switch (Bg.Mode) {
        case ModeBG::None:
            FastLED.clear();
            break;
        case ModeBG::SolidColor:
            bg_solidColor(Bg);
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

    // update the frame
    switch (Fr.Mode) {
        case ModeFR::None:
            break;
        case ModeFR::Time:
            now = RTC_TIME.now();
            fr_time(now, Fr);
            break;
        case ModeFR::SolidColor:
            fr_solidColor(Fr);
        default:
            break;
    }

    // update the foreground
    switch (Fg.Mode) {
        case ModeFG::Time:
        case ModeFG::TimeRainbow:
            now = RTC_TIME.now();
            disp_time(now, Fg);

            break;
        case ModeFG::None:  // No operation
            break;
        case ModeFG::Cycle:
            disp_number((cycle_counter / 1000) % 10, (cycle_counter / 100) % 10, (cycle_counter / 10) % 10, cycle_counter % 10, Fg);
            cycle_counter++;
            if (cycle_counter >= 10000)
                cycle_counter = 0;
            break;
        default:
            break;
    }

    // Display warnings/Errors
    for (int i = 0; i < (sizeof(ErrorIndicator) / sizeof(ErrorIndicator[0])); i++) {
        switch (ErrorIndicator[i]) {
            case 1:  // warning
                leds[ErrorIndicatorAdr[i]] = CRGB::DarkOrange;
                break;
            case 2:  // error
                leds[ErrorIndicatorAdr[i]] = CRGB::Red;
                break;
        }
    }
    FastLED.show();
}

//=====PRIVATE====================================================================================
/** ================ FOREGROUND ================ **/

void PLedDisp::disp_time(DateTime &time, Foreground &fg) {
    if ((fg.Mode == ModeFG::TimeRainbow) || (fg.Mode == ModeFG::Cycle)) {
        if (bg_counter < REFRESH_RATE_HZ / 4)
            bg_counter++;
        else {
            bg_colour.hue = (bg_colour.hue + 1) % 256;
            bg_counter = 0;
        }
    }

    // Write Digits
    disp_digit(time.hour() / 10, 0, fg);          // 1. Digit 10Hours
    disp_digit(time.hour() % 10, 28, fg);         // 2. Digit 1Hour
    disp_digit(time.minute() / 10, 70, fg);       // 3. Digit 10 Min
    disp_digit(time.minute() % 10, 70 + 28, fg);  // 4. Digit 1Min

    // seconds tick ":" between Digit 2 and 3 refreshed all 2 seconds
    if (time.second() % 2 == 0) {
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

void PLedDisp::disp_number(uint8_t Digit3, uint8_t Digit2, uint8_t Digit1, uint8_t Digit0, Foreground &fg) {
    // Write Digits
    int NbrForDisplay = Digit3 * 1000 + Digit2 * 100 + Digit1 * 10 + Digit0 * 1;

    // Hide leading zero
    if (NbrForDisplay >= 1000) {
        disp_digit(Digit3, 14, fg);
    }
    if (NbrForDisplay >= 100) {
        disp_digit(Digit2, 42, fg);
    }
    if (NbrForDisplay >= 10) {
        disp_digit(Digit1, 70, fg);
    }
    if (NbrForDisplay >= 0) {
        disp_digit(Digit0, 70 + 28, fg);
    }
}

void PLedDisp::disp_digit(int num, int offset, Foreground &fg) {
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

CRGB PLedDisp::fg_palette(int indx, Foreground &fg) {
    // Check if index is valid
    if (indx < 0 && indx >= NUM_LEDS) {
        return CRGB::Black;
    }
    // Mode Scrolling Rainbow Time or Cyclic
    if ((fg.Mode == ModeFG::TimeRainbow) || (fg.Mode == ModeFG::Cycle)) {
        return CHSV((bg_colour.hue + indx) % 256, bg_colour.sat, bg_colour.val);
    }

    return fg.Color;
}
void PLedDisp::fr_solidColor(Frame &fr) {
    for (int i = 0; i < sizeof(frame) / sizeof(frame[0]); i++) {
        if ((frame[i] >= 0) and (frame[i] <= (sizeof(leds) / sizeof(leds[0])))) {
            leds[frame[i]] = fr.Color;
        }
    }
}

void PLedDisp::fr_time(DateTime &time, Frame &fr) {
    int framelength = sizeof(frame) / sizeof(frame[0]);
    int length = ((time.second() * double(framelength)) / 59);

    if (length < 0) {
        length = 0;
    } else if (length > framelength) {
        length = framelength;
    }

    for (int i = 0; i < length; i++) {
        if ((frame[i] >= 0) and (frame[i] <= (sizeof(leds) / sizeof(leds[0])))) {
            leds[frame[i]] = fr.Color;
        }
    }
}

/** ================ BACKGROUND ================ **/
void PLedDisp::bg_solidColor(Background &bg) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = bg.Color;
    }
}
void PLedDisp::bg_rainbow() {
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

void PLedDisp::bg_twinkle() {
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

void PLedDisp::bg_rain() {
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

void PLedDisp::bg_firework() {
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

void PLedDisp::bg_firepit() {
    for (int level = 6; level > 2; level--) {
        for (int i = 0; i < 17 + (6 - level); i++) {
            leds[led_address[level][i]] = CHSV(HUE_RED + random8(8), 255, random8(192 - (6 - level) * 64, 255 - (6 - level) * 64));
        }
    }
}