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

#include "LogConfiguration.h"
#include "PLedDisp/PLedDisp.h"
#include "WlanConfiguration.h"

// Global Time keeping
RTC_Millis RTC_TIME;
DateTime TIME_NOW;

// Replace with your network credentials
const char* ssid = DEFAULT_WIFI_SSID;            // "SSID"
const char* password = DEFAULT_WIFI_PASSWORD;    // "PASSWORD"
const char* poolServerName = "ch.pool.ntp.org";  // "time.nist.gov"

// Time constants for easyier calculation
const uint TIME_SECONDSINMINUTE = 60;
const uint TIME_SECONDSINHOUR = 60 * TIME_SECONDSINMINUTE;
const uint TIME_SECONDSINDAY = 24 * TIME_SECONDSINHOUR;

// Define NTP Client to get time
WiFiUDP ntpUDP;

// Set offset time in seconds to adjust for your timezone, for example:
// GMT +1 = 3600
// GMT +8 = 28800
// GMT -1 = -3600
// GMT 0 = 0
const int ntpTimeOffset = +1 * TIME_SECONDSINHOUR;  // [sec] GMT +1
const int ntpUpdateInterval = 5 * 60 * 1000;        // [ms] 5min
NTPClient timeClient(ntpUDP, poolServerName, ntpTimeOffset, ntpUpdateInterval);

PLedDisp* pleddisp;  ///< Instance

struct StateMachine {
    bool doInitAction = true;
    uint actualState = 0;
    uint oldState = 99;
};

// Statemachine to set behavior via serial
StateMachine SmaSerial;
enum class StateSerial { Idle,
                         SetBackground,
                         SetForeground,
                         SetFrame,
                         Update };
void UpdateSerialSma();
char mode_fg = 'n';  // Mode Foreground
char mode_fr = 'n';  // Mode Frame
char mode_bg = 'n';  // Mode Background

// Statemachine to control behavior via time
StateMachine SmaTime;
enum class StateTime { Idle,
                       Morning,
                       Day,
                       Evening,
                       Night };
void UpdateTimeSma();

uint NbrRepeatTrainAnimation = 0;

/**
 * @brief Display a frame in 4 steps/different color as indicator for a timer.
 * Needs to be called every second.
 *
 * @param timeSecondsPassedInDay - Time now in seconds since 00:00 of this day
 * @param timeSecondsNextAlarm - Time when the timer ends in seconds since 00:00 of this day
 * @return true - Timer finished
 * @return false - Timer still running
 */
bool SetTimerAnimation(uint timeSecondsPassedInDay, uint timeSecondsNextAlarm);

/**
 * The setup method used by the Arduino.
 */
void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // wait for serial port to connect. Needed for native USB port only
    }
    DBPrintln("==Start Setup==");

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

    UpdateSerialSma();

    StatusNtpOk = timeClient.update();
    StatusWlanOk = (WiFi.status() == WL_CONNECTED);
    if (StatusNtpOk) {
        RTC_TIME.adjust(DateTime(timeClient.getEpochTime()));
    }

    UpdateTimeSma();

    pleddisp->setWarning(0, StatusWlanOk, 2);
    pleddisp->setWarning(1, StatusNtpOk);
    pleddisp->setWarning(2, true, 2);
    pleddisp->setWarning(3, true);
    pleddisp->update_LEDs();
}

void UpdateSerialSma() {
    switch (SmaSerial.actualState) {
        SmaSerial.doInitAction = (SmaSerial.oldState != SmaSerial.actualState);
        SmaSerial.actualState = SmaSerial.oldState;
        case uint(StateSerial::Idle):
            if (SmaSerial.doInitAction) {
            }
            break;
        case uint(StateSerial::SetForeground):
            if (SmaSerial.doInitAction) {
                Serial.println("Set Foreground Mode:");
                Serial.println("'N' no op (time doesn't show)");
                Serial.println("'T' time");
                Serial.println("'R' rainbow time");
                Serial.println("'C' cycle through all digits");
            }

            mode_fg = Serial.read();
            if (((mode_fg == 'N') or (mode_fg == 'n')) or
                ((mode_fg == 'T') or (mode_fg == 't')) or
                ((mode_fg == 'R') or (mode_fg == 'r')) or
                ((mode_fg == 'C') or (mode_fg == 'c'))) {
                Serial.println(mode_fg);
                SmaSerial.actualState = uint(StateSerial::SetFrame);
            }
            break;
        case uint(StateSerial::SetFrame):
            if (SmaSerial.doInitAction) {
                Serial.println("Set Frame Mode");
                Serial.println("'N' No background");
                Serial.println("'S' One color");
                Serial.println("'T' time");
            }

            mode_fr = Serial.read();
            if (((mode_fr == 'N') or (mode_fr == 'n')) or
                ((mode_fr == 'T') or (mode_fr == 't')) or
                ((mode_fr == 'S') or (mode_fr == 's'))) {
                Serial.println(mode_fr);
                SmaSerial.actualState = uint(StateSerial::SetBackground);
            }
            break;
        case uint(StateSerial::SetBackground):
            if (SmaSerial.doInitAction) {
                Serial.println("Set Background Mode");
                Serial.println("'N' No background");
                Serial.println("'S' One color");
                Serial.println("'R' Scrolling rainbow background");
                Serial.println("'W' Twinkle");
                Serial.println("'F' Fireworks");
                Serial.println("'T' Thunderstorm");
                Serial.println("'P' Firepit");
            }

            mode_bg = Serial.read();
            if (((mode_bg == 'N') or (mode_bg == 'n')) or
                ((mode_bg == 'S') or (mode_bg == 's')) or
                ((mode_bg == 'R') or (mode_bg == 'r')) or
                ((mode_bg == 'W') or (mode_bg == 'w')) or
                ((mode_bg == 'F') or (mode_bg == 'f')) or
                ((mode_bg == 'T') or (mode_bg == 't')) or
                ((mode_bg == 'P') or (mode_bg == 'p'))) {
                Serial.println(mode_bg);
                SmaSerial.actualState = uint(StateSerial::Update);
            }
            break;

        case uint(StateSerial::Update):
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
            SmaSerial.actualState = uint(StateSerial::Idle);
            Serial.println("----------------------------------");
            break;
        default:
            break;
    }
}

void UpdateTimeSma() {
    TIME_NOW = RTC_TIME.now();
    uint timeSecondsPassedInDay = TIME_NOW.unixtime() % TIME_SECONDSINDAY;
    bool DayIsWeekend = ((TIME_NOW.dayOfTheWeek() == 6) || (TIME_NOW.dayOfTheWeek() == 0));
    const uint timeStartRoutineNight = 1 * TIME_SECONDSINMINUTE;                                       //[sec] 0:01
    const uint timeStartRoutineMorning = 6.5 * TIME_SECONDSINHOUR;                                     //[sec] 6:30
    const uint timeStartRoutineMorningFirstTrain = 7 * TIME_SECONDSINHOUR + 1 * TIME_SECONDSINMINUTE;  //[sec] 7:01
    const uint timeStartRoutineDay = 8.5 * TIME_SECONDSINHOUR;                                         //[sec] 8:30
    const uint timeStartRoutineEvening = 17.50 * TIME_SECONDSINHOUR;                                   //[sec] 17:30
    const uint brightnessDay = 80;
    const uint brightnessNight = 5;

    SmaTime.doInitAction = (SmaTime.oldState != SmaTime.actualState);
    SmaTime.oldState = SmaTime.actualState;
    if (SmaTime.doInitAction) {
        DBPrintln(SmaTime.actualState);
        DBPrintln(timeSecondsPassedInDay);
        DBPrintln(timeSecondsPassedInDay / 60.0 / 60);
    }
    switch (SmaTime.actualState) {
        case uint(StateTime::Idle):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Idle");
                // Set defaults
                pleddisp->setForegroundColor(CRGB::Peru);
                pleddisp->setBrightness(brightnessDay);
            }

            if ((timeSecondsPassedInDay >= timeStartRoutineNight) and (timeSecondsPassedInDay < timeStartRoutineMorning)) {
                SmaTime.actualState = uint(StateTime::Night);
                break;
            }
            if ((timeSecondsPassedInDay >= timeStartRoutineMorning) and (timeSecondsPassedInDay < timeStartRoutineDay)) {
                SmaTime.actualState = uint(StateTime::Morning);
                break;
            }
            if ((timeSecondsPassedInDay >= timeStartRoutineDay) and (timeSecondsPassedInDay < timeStartRoutineEvening)) {
                SmaTime.actualState = uint(StateTime::Day);
                break;
            }
            if (timeSecondsPassedInDay >= timeStartRoutineEvening) {
                SmaTime.actualState = uint(StateTime::Evening);
                break;
            }

            break;
        case uint(StateTime::Morning):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Morning");
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                pleddisp->setBrightness(brightnessDay);

                NbrRepeatTrainAnimation = 0;
            }

            if (NbrRepeatTrainAnimation < 4) {
                bool AnimationFinished = SetTimerAnimation(timeSecondsPassedInDay,
                                                           timeStartRoutineMorningFirstTrain + (NbrRepeatTrainAnimation * 15 * TIME_SECONDSINMINUTE));
                if (AnimationFinished) {
                    NbrRepeatTrainAnimation++;
                    DBPrint("NbrRepeatTrainAnimation: ");
                    DBPrintln(NbrRepeatTrainAnimation);
                }
            }
            if (timeSecondsPassedInDay >= timeStartRoutineDay) {
                SmaTime.actualState = uint(StateTime::Day);
                break;
            }
            break;
        case uint(StateTime::Day):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Day");
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                if (DayIsWeekend) {
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                } else {
                    pleddisp->setForegroundMode(PLedDisp::ModeFG::None);
                }
                pleddisp->setBrightness(brightnessDay);
            }

            if (timeSecondsPassedInDay >= timeStartRoutineEvening) {
                SmaTime.actualState = uint(StateTime::Evening);
                break;
            }
            break;
        case uint(StateTime::Evening):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Evening");
            }

            if ((timeSecondsPassedInDay >= timeStartRoutineNight) && (timeSecondsPassedInDay < timeStartRoutineMorning)) {
                SmaTime.actualState = uint(StateTime::Night);
                break;
            }
            break;
        case uint(StateTime::Night):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Night");
                // Turn off
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                pleddisp->setBrightness(brightnessNight);
            }

            if (timeSecondsPassedInDay == timeStartRoutineMorning) {
                SmaTime.actualState = uint(StateTime::Morning);
                break;
            }
            break;

        default:
            break;
    }
}

bool SetTimerAnimation(uint timeSecondsPassedInDay, uint timeSecondsNextAlarm) {
    const uint timeLeftInfo = 6 * TIME_SECONDSINMINUTE;     // 6 minutes left - Info
    const uint timeLeftWarning = 4 * TIME_SECONDSINMINUTE;  // 4 minutes left - Warning
    const uint timeLeftRunning = 3 * TIME_SECONDSINMINUTE;  // 3 minutes left - RUN!
    const uint timeLeftStop = 2 * TIME_SECONDSINMINUTE;     // 2 minutes left - Too late

    int timeLeft = timeSecondsNextAlarm - timeSecondsPassedInDay;

    if (timeLeft < timeLeftStop) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::None);
    } else if (timeLeft < timeLeftRunning) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::Red);
    } else if (timeLeft < timeLeftWarning) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::DarkOrange);
    } else if (timeLeft < timeLeftInfo) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::LightBlue);
    }

    return (timeLeft == 0);
}