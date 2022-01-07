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
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "PLedDisp/PLedDisp.h"
#include "WlanConfiguration.h"

RTC_Millis RTC_TIME;  // Global Time keeping

// Replace with your network credentials
const char* ssid = DEFAULT_WIFI_SSID;            // "SSID"
const char* password = DEFAULT_WIFI_PASSWORD;    // "PASSWORD"
const char* poolServerName = "ch.pool.ntp.org";  // "time.nist.gov"

// Define NTP Client to get time
WiFiUDP ntpUDP;

// Set offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 3600
// GMT +8 = 28800
// GMT -1 = -3600
// GMT 0 = 0
int ntpTimeOffset = 3600;               // Seconds
int ntpUpdateInterval = 5 * 60 * 1000;  // ms
NTPClient timeClient(ntpUDP, poolServerName, ntpTimeOffset, ntpUpdateInterval);

PLedDisp* pleddisp;  ///< Instance

uint smaStep = 0;

/**
 * The setup method used by the Arduino.
 */
void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // wait for serial port to connect. Needed for native USB port only
    }
    WiFi.begin(ssid, password);

    // while (WiFi.status() != WL_CONNECTED) {
    //     // wait for wlan to connect
    //     delay(500);
    //     Serial.print(".");
    // }

    timeClient.begin();
    RTC_TIME.begin(DateTime(F(__DATE__), F(__TIME__)));
    pleddisp = new PLedDisp();
}

/**
 * The main runloop used by the Arduino.
 */
void loop() {
    bool StatusNtpOk;
    bool StatusWlanOk;
    char mode_fg;  //
    char mode_fr;  //
    char mode_bg;  //
    switch (smaStep) {
        case 0:
            // Idle
            smaStep = 0;
            break;
        case 10:
            Serial.println("Set Foreground Mode:");
            Serial.println("'N' no op (time doesn't show)");
            Serial.println("'T' time");
            Serial.println("'R' rainbow time");
            Serial.println("'C' cycle through all digits");
            smaStep = 11;
            break;
        case 11:
            mode_fg = Serial.read();
            if (((mode_fg == 'N') or (mode_fg == 'n')) or
                ((mode_fg == 'T') or (mode_fg == 't')) or
                ((mode_fg == 'R') or (mode_fg == 'r')) or
                ((mode_fg == 'C') or (mode_fg == 'c'))) {
                Serial.println(mode_fg);
                smaStep = 20;
            }
            break;
        case 20:
            Serial.println("Set Frame Mode");
            Serial.println("'N' No background");
            Serial.println("'S' One color");
            Serial.println("'T' time");
            smaStep = 21;
            break;
        case 21:
            mode_fr = Serial.read();
            if (((mode_fr == 'N') or (mode_fr == 'n')) or
                ((mode_fr == 'T') or (mode_fr == 't')) or
                ((mode_fr == 'S') or (mode_fr == 's'))) {
                Serial.println(mode_fr);
                smaStep = 30;
            }
            break;
        case 30:
            Serial.println("Set Background Mode");
            Serial.println("'N' No background");
            Serial.println("'S' One color");
            Serial.println("'R' Scrolling rainbow background");
            Serial.println("'W' Twinkle");
            Serial.println("'F' Fireworks");
            Serial.println("'T' Thunderstorm");
            Serial.println("'P' Firepit");
            smaStep = 31;
            break;
        case 31:
            // while (Serial.available() == 0) {
            // };
            mode_bg = Serial.read();
            if (((mode_bg == 'N') or (mode_bg == 'n')) or
                ((mode_bg == 'S') or (mode_bg == 's')) or
                ((mode_bg == 'R') or (mode_bg == 'r')) or
                ((mode_bg == 'W') or (mode_bg == 'w')) or
                ((mode_bg == 'F') or (mode_bg == 'f')) or
                ((mode_bg == 'T') or (mode_bg == 't')) or
                ((mode_bg == 'P') or (mode_bg == 'p'))) {
                Serial.println(mode_bg);
                smaStep = 110;
            }
            break;

        case 110:
            switch (mode_fg) {
                case 'N':
                case 'n':
                    Serial.println("FG: None");
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::None);
                    break;
                case 'T':
                case 't':
                    Serial.println("FG: Time");
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::Time);
                    break;
                case 'R':
                case 'r':
                    Serial.println("FG: TimeRainbow");
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::TimeRainbow);
                    break;
                case 'C':
                case 'c':
                    Serial.println("FG: Cycle");
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::Cycle);
                    break;
                default:
                    Serial.println(mode_fg);
                    Serial.println("FG: DEFAULT");
                    break;
            }
            smaStep = 120;
            break;

        case 120:
            switch (mode_fr) {
                case 'N':
                case 'n':
                    Serial.println("FR: None");
                    pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                    break;
                case 'T':
                case 't':
                    Serial.println("FR: Time");
                    pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
                    break;
                case 'S':
                case 's':
                    Serial.println("FR: SolidColor");
                    pleddisp->setFrameMode(PLedDisp::ModeFR::SolidColor);
                    break;
                default:
                    Serial.println(mode_fr);
                    Serial.println("FR: DEFAULT");
                    break;
            }
            smaStep = 130;
            break;

        case 130:
            switch (mode_bg) {
                case 'N':
                case 'n':
                    Serial.println("BG: None");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                    break;
                case 'S':
                case 's':
                    Serial.println("BG: SolidColor");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::SolidColor);
                    break;
                case 'R':
                case 'r':
                    Serial.println("BG: ScrollingRainbow");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::ScrollingRainbow);
                    break;
                case 'W':
                case 'w':
                    Serial.println("BG: Twinkle");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::Twinkle);
                    break;
                case 'F':
                case 'f':
                    Serial.println("BG: Thunderstorm");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::Fireworks);
                    break;
                case 'T':
                case 't':
                    Serial.println("BG: Thunderstorm");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::Thunderstorm);
                    break;
                case 'P':
                case 'p':
                    Serial.println("BG: Firepit");
                    pleddisp->setBackgroundMode(PLedDisp::ModeBG::Firepit);
                    break;
                default:
                    Serial.println(mode_bg);
                    Serial.println("BG: DEFAULT");
                    break;
            }
            smaStep = 0;
            Serial.println("----------------------------------");
            break;
        default:
            break;
    }

    StatusNtpOk = timeClient.update();
    StatusWlanOk = (WiFi.status() == WL_CONNECTED);
    if (StatusNtpOk) {
        RTC_TIME.adjust(DateTime(timeClient.getEpochTime()));
    }
    pleddisp->setWarning(0, StatusWlanOk, 2);
    pleddisp->setWarning(1, StatusNtpOk);
    pleddisp->setWarning(2, true, 2);
    pleddisp->setWarning(3, true);
    pleddisp->update_LEDs();
}
