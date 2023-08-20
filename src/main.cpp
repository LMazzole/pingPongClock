/**
 * @file main.cpp
 * @brief PingPong-LedClock control with multiple features:
 *  NTP-Synchronization for timekeeping without RTC
 *  Recycling-Remineder for the next day
 *  Timer/Indicator for train-departure in the morning
 *  Philips-hue connectivity for presence-control
 *
 * Build for an ESP32
 *
 * @author L M
 * @date 2022-01-23
 *
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Timezone.h>  // https://github.com/JChristensen/Timezone
#include <WiFi.h>
#include <WiFiUdp.h>

#include "LogConfiguration.h"
#include "PLedDisp/PLedDisp.h"
#include "secrets.h"

// Global Time keeping
RTC_Millis RTC_TIME;
DateTime TIME_NOW;

// Time constants for easyier calculation
const uint TIME_MINUTEINSECONDS = 60;
const uint TIME_HOURINSECONDS = 60 * TIME_MINUTEINSECONDS;
const uint TIME_DAYINSECONDS = 24 * TIME_HOURINSECONDS;

// Define NTP Client to get time
WiFiUDP ntpUDP;

WiFiClient wifi;

const int ntpTimeOffset = 0 * TIME_HOURINSECONDS;  // [sec] 0 because Timezone will update
const int ntpUpdateInterval = 5 * 60 * 1000;       // [ms] 5min
NTPClient timeClient(ntpUDP, NTP_SERVERNAME, ntpTimeOffset, ntpUpdateInterval);

// Central European Time (Frankfurt, Paris) GMT +1
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};  // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone CE(CEST, CET);

PLedDisp* pleddisp;  ///< Instance

//===RTOS===
TaskHandle_t TaskMain;
void TaskMainCode(void* pvParameters);
TaskHandle_t TaskLcd;
void TaskLcdCode(void* pvParameters);
TaskHandle_t TaskTime;
void TaskTimeHandlingCode(void* pvParameters);
TaskHandle_t TaskMqtt;
void TaskMqttCode(void* pvParameters);

//==============================================================================================

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

//==============================================================================================

/**
 * @brief Display a frame in 4 steps/different color as indicator for a timer.
 * Needs to be called every second.
 *
 * @param timeSecondsPassedInDay - Time now in seconds since 00:00 of this day
 * @param timeSecondsTimerEnds - Time when the timer ends in seconds since 00:00 of this day
 * @return true - Timer finished
 * @return false - Timer still running
 */
bool SetTimerAnimation(uint timeSecondsPassedInDay, uint timeSecondsNextAlarm);

enum class Recycling { None,
                       Paper,
                       Cardboard,
                       Metal
};

/**
 * @brief Check if and what needs to be recycled tomorrow
 *
 * @return enum Recycling - Typ of recycling
 */
enum Recycling CheckDateForRecycling();

//==============================================================================================
unsigned long currentMillis = 0;           ///< Current time for non blocking delay
unsigned long previousMillisMovement = 0;  ///< Last time called for non blocking delay

//==============================================================================================

uint NbrRepeatTrainAnimation = 0;
uint uindebugTimeMs = 0;

//=====MQTT=========================================================================================
void callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqttClient(wifi);
bool reconnect();
bool ClockInManualMode = false;
// Dates for recycling {DD,MM}
uint8_t recyclingCardboardDates[19][2];
long recyclingCardboardColor = CRGB::Beige;
uint8_t recyclingPaperDates[19][2];
long recyclingPaperColor = CRGB::WhiteSmoke;
uint8_t recyclingMetalDates[19][2];
long recyclingMetalColor = CRGB::MediumBlue;
//==============================================================================================

/**
 * The setup method used by the Arduino.
 */
void setup() {
    Serial.begin(115200);
    while (!Serial) {
        // wait for serial port to connect. Needed for native USB port only
    }
    DBPrintln("== Setup: Start ==");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    DBPrintln("Establishing WLAN connection");
    while (WiFi.status() != WL_CONNECTED) {
        // wait for wlan to connect
        delay(500);
        DBPrint(".");
    }
    DBPrintln("")
        DBPrintln("WLAN connected.");

    timeClient.begin();
    RTC_TIME.begin(DateTime(F(__DATE__), F(__TIME__)));
    pleddisp = new PLedDisp();

    mqttClient.setServer(MQTT_SERVER, 1883);
    mqttClient.setCallback(callback);

    //===RTOS===
    xTaskCreate(
        TaskTimeHandlingCode, /* Function to implement the task */
        "TaskTime",           /* Name of the task */
        10000,                /* Stack size in words */
        NULL,                 /* Task input parameter */
        1,                    /* Priority of the task */
        &TaskTime);           /* Task handle. */
    // 0);                   /* Core where the task should run */
    delay(500);

    xTaskCreate(
        TaskMainCode, /* Function to implement the task */
        "TaskMain",   /* Name of the task */
        10000,        /* Stack size in words */
        NULL,         /* Task input parameter */
        1,            /* Priority of the task */
        &TaskMain);   /* Task handle. */
    // 1);           /* Core where the task should run */
    delay(500);

    xTaskCreate(
        TaskLcdCode, /* Function to implement the task */
        "TaskLcd",   /* Name of the task */
        10000,       /* Stack size in words */
        NULL,        /* Task input parameter */
        2,           /* Priority of the task. 0 = lowest */
        &TaskLcd);   /* Task handle. */
    // 0);          /* Core where the task should run */
    delay(500);

    xTaskCreate(
        TaskMqttCode, /* Function to implement the task */
        "TaskMqtt",   /* Name of the task */
        10000,        /* Stack size in words */
        NULL,         /* Task input parameter */
        2,            /* Priority of the task. 0 = lowest */
        &TaskMqtt);   /* Task handle. */
    // 0);           /* Core where the task should run */
    delay(500);

    DBPrintln("== Setup: End ==");
}

/**
 * Task for updating time
 * Runs every 20 ms on core 0
 */
void TaskTimeHandlingCode(void* pvParameters) {
    DBPrint("TaskTimeHandlingCode running on core ");
    DBPrintln(xPortGetCoreID());

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = 20;  // ms
    bool StatusNtpOk;

    for (;;) {
        // Wait for the next cycle
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        StatusNtpOk = timeClient.update();
        if (StatusNtpOk) {
            RTC_TIME.adjust(DateTime(CE.toLocal(timeClient.getEpochTime())));
        }

        TIME_NOW = RTC_TIME.now();
    }
}

/**
 * Task for updating display mode
 * Runs every 5 seconds on core 1
 */
void TaskMainCode(void* pvParameters) {
    DBPrint("TaskMainCode running on core ");
    DBPrintln(xPortGetCoreID());

    const TickType_t xFrequency = 1 * 1000;  // sec
    TickType_t xLastWakeTime = xTaskGetTickCount();

    bool StatusWlanOk;
    for (;;) {
        // Wait for the next cycle
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        StatusWlanOk = (WiFi.status() == WL_CONNECTED);

        if (StatusWlanOk) {
        }

        // Set and Update warning LED's
        pleddisp->setWarning(0, StatusWlanOk, 2);
        // pleddisp->setWarning(1, StatusNtpOk);
        pleddisp->setWarning(2, true, 2);

        if (ClockInManualMode == false) {
            UpdateTimeSma();
        } else {
            SmaTime.oldState = 99;  // Trigger initOnce again
        }

        // UpdateSerialSma();
        // if (SleepActive) {
        //     pleddisp->setBrightness(0);
        // }
    }
}

/**
 * Task for updating display
 * Runs every 50ms on core 0
 */
void TaskLcdCode(void* pvParameters) {
    DBPrint("TaskLcdCode running on core ");
    DBPrintln(xPortGetCoreID());

    const TickType_t xFrequency = 50;  // ms
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Wait for the next cycle
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        pleddisp->update_LEDs();
    }
}

/**
 * Task for updating display
 * Runs every 50ms on core 0
 */
void TaskMqttCode(void* pvParameters) {
    DBPrint("TaskMqttCode running on core ");
    DBPrintln(xPortGetCoreID());

    const TickType_t xFrequency = 50;  // ms
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Wait for the next cycle
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (!mqttClient.connected()) {
            reconnect();
        }
        mqttClient.loop();
    }
}

/**
 * ideal task
 */
void loop() {
}

//==============================================================================================

void UpdateSerialSma() {
    switch (SmaSerial.actualState) {
        SmaSerial.doInitAction = (SmaSerial.oldState != SmaSerial.actualState);
        SmaSerial.oldState = SmaSerial.actualState;

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
    uint timeSecondsPassedInDay = TIME_NOW.unixtime() % TIME_DAYINSECONDS;
    bool DayIsWeekend = ((TIME_NOW.dayOfTheWeek() == 6) || (TIME_NOW.dayOfTheWeek() == 0));

    // Define starttimes for different routines throuh the day
    const uint timeStartRoutineNight = 1 * TIME_MINUTEINSECONDS;                                       //[sec] 0:01
    const uint timeStartRoutineMorning = 6.5 * TIME_HOURINSECONDS;                                     //[sec] 6:30
    const uint timeStartRoutineMorningFirstTrain = 7 * TIME_HOURINSECONDS + 1 * TIME_MINUTEINSECONDS;  //[sec] 7:01
    const uint timeStartRoutineDay = 8.5 * TIME_HOURINSECONDS;                                         //[sec] 8:30
    const uint timeStartRoutineEvening = 17.50 * TIME_HOURINSECONDS;                                   //[sec] 17:30
    const uint brightnessHigh = 70;
    const uint brightnessLow = 10;

    if ((timeSecondsPassedInDay >= timeStartRoutineNight) and (timeSecondsPassedInDay < timeStartRoutineMorning)) {
        SmaTime.actualState = uint(StateTime::Night);
    } else if ((timeSecondsPassedInDay >= timeStartRoutineMorning) and (timeSecondsPassedInDay < timeStartRoutineDay)) {
        SmaTime.actualState = uint(StateTime::Morning);
    } else if ((timeSecondsPassedInDay >= timeStartRoutineDay) and (timeSecondsPassedInDay < timeStartRoutineEvening)) {
        SmaTime.actualState = uint(StateTime::Day);
    } else if ((timeSecondsPassedInDay >= timeStartRoutineEvening) or (timeSecondsPassedInDay < timeStartRoutineNight)) {
        SmaTime.actualState = uint(StateTime::Evening);
    }

    SmaTime.doInitAction = (SmaTime.oldState != SmaTime.actualState);
    SmaTime.oldState = SmaTime.actualState;
    if (SmaTime.doInitAction) {
        DBPrint("actualState");
        DBPrintln(SmaTime.actualState);
        DBPrint("Time: Seconds in Day passed:");
        DBPrint(timeSecondsPassedInDay);
        DBPrint(" -> ");
        DBPrint(timeSecondsPassedInDay / 60.0 / 60);
        DBPrintln("h");
    }

    switch (SmaTime.actualState) {
        case uint(StateTime::Idle):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Idle");
                // Set defaults
                pleddisp->setForegroundColor(CRGB::Peru);
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                // pleddisp->setBrightness(brightnessHigh);
            }

            break;

        case uint(StateTime::Morning):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Morning");
                NbrRepeatTrainAnimation = 0;

                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
            }
            // pleddisp->setBrightness(brightnessHigh);

            if (NbrRepeatTrainAnimation < 4) {
                uint timeAlarmForNextTrain = timeStartRoutineMorningFirstTrain;                  // DayTime s when next train leaves
                timeAlarmForNextTrain += (NbrRepeatTrainAnimation * 15 * TIME_MINUTEINSECONDS);  // Train leaves every 15 Minutes
                timeAlarmForNextTrain -= (3 * TIME_MINUTEINSECONDS);                             // Alarmtime 3Minute before train leaves
                bool AnimationFinished = SetTimerAnimation(timeSecondsPassedInDay, timeAlarmForNextTrain);
                if (AnimationFinished) {
                    NbrRepeatTrainAnimation++;
                    DBPrint("NbrRepeatTrainAnimation: ");
                    DBPrintln(NbrRepeatTrainAnimation);
                }
            }
            break;
        case uint(StateTime::Day):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Day");

                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                // if (DayIsWeekend) {
                //     pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                // } else {
                //     pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                // }
            }
            // pleddisp->setBrightness(brightnessHigh);

            break;
        case uint(StateTime::Evening):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Evening");
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
                // Check for ToDoTasks for the next day
                switch (CheckDateForRecycling()) {
                    case Recycling::Cardboard:
                        pleddisp->setFrameMode(PLedDisp::ModeFR::SolidColor);
                        pleddisp->setFrameColor(recyclingCardboardColor);
                        break;
                    case Recycling::Paper:
                        pleddisp->setFrameMode(PLedDisp::ModeFR::SolidColor);
                        pleddisp->setFrameColor(recyclingPaperColor);
                        break;
                    case Recycling::Metal:
                        pleddisp->setFrameMode(PLedDisp::ModeFR::SolidColor);
                        pleddisp->setFrameColor(recyclingMetalColor);
                        break;
                    default:
                        break;
                };
            }
            // pleddisp->setBrightness(brightnessHigh);

            break;
        case uint(StateTime::Night):
            if (SmaTime.doInitAction) {
                DBPrintln("StateTime::Night");
                // Turn off
                pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
                pleddisp->setFrameMode(PLedDisp::ModeFR::None);
                pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, true);
            }
            // pleddisp->setBrightness(brightnessLow);

            break;

        default:
            DBPrintln("StateTime::default");
            DBPrintln(SmaTime.actualState);
            break;
    }
}

//=====================================================================================

bool SetTimerAnimation(uint timeSecondsPassedInDay, uint timeSecondsTimerEnds) {
    const uint timeLeftIndicator1 = 6 * TIME_MINUTEINSECONDS;  // Info
    const uint timeLeftIndicator2 = 3 * TIME_MINUTEINSECONDS;  // Warning
    const uint timeLeftIndicator3 = 1 * TIME_MINUTEINSECONDS;  // Last Indicator

    if (timeSecondsTimerEnds < timeSecondsPassedInDay) {
        // Timer endet
        pleddisp->setFrameMode(PLedDisp::ModeFR::None);
        return true;
    }

    int timeLeft = timeSecondsTimerEnds - timeSecondsPassedInDay;

    if (timeLeft < timeLeftIndicator3) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::Red);
    } else if (timeLeft < timeLeftIndicator2) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::DarkOrange);
    } else if (timeLeft < timeLeftIndicator1) {
        pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        pleddisp->setFrameColor(CRGB::LightBlue);
    }

    return false;
}

enum Recycling CheckDateForRecycling() {
    DBPrintln("CheckDateForRecycling");
    DateTime tomorrow = (TIME_NOW + TIME_DAYINSECONDS);
    uint8_t checkDate[][2] = {{tomorrow.day(),
                               tomorrow.month()}};

    // Dates for recycling {DD,MM}
    // const uint8_t datesCardboard[][2] = {{4, 1}, {1, 2}, {1, 3}, {29, 3}, {26, 4}, {24, 5}, {21, 6}, {19, 7}, {16, 8}, {13, 9}, {11, 10}, {15, 11}, {13, 12}};  // DD,MM
    // const uint8_t datesPaper[][2] = {{25, 1}, {22, 2}, {22, 3}, {19, 4}, {17, 5}, {14, 6}, {12, 7}, {9, 8}, {6, 9}, {4, 10}, {8, 11}, {6, 12}};                 // DD,MM
    // const uint8_t datesMetal[][2] = {{15, 12}};                                                                                                                 // DD,MM

    for (int i = 0; i < (sizeof(recyclingCardboardDates) / sizeof(recyclingCardboardDates[0])); i++) {
        if ((recyclingCardboardDates[i][0] == checkDate[0][0]) &&
            (recyclingCardboardDates[i][1] == checkDate[0][1])) {
            DBPrintln("Tomorrow is recycling: Cardboard");
            return Recycling::Cardboard;
        }
    }

    for (int i = 0; i < (sizeof(recyclingPaperDates) / sizeof(recyclingPaperDates[0])); i++) {
        if ((recyclingPaperDates[i][0] == checkDate[0][0]) &&
            (recyclingPaperDates[i][1] == checkDate[0][1])) {
            DBPrintln("Tomorrow is recycling: Paper");
            return Recycling::Paper;
        }
    }

    for (int i = 0; i < (sizeof(recyclingMetalDates) / sizeof(recyclingMetalDates[0])); i++) {
        if ((recyclingMetalDates[i][0] == checkDate[0][0]) &&
            (recyclingMetalDates[i][1] == checkDate[0][1])) {
            DBPrintln("Tomorrow is recycling: Metal");
            return Recycling::Metal;
        }
    }

    return Recycling::None;
}

void callback(char* topic, byte* payload, unsigned int inputLength) {
    DBPrintln("===========================================");
    DBPrint("Message arrived on topic: ");
    DBPrintln(topic);
    // StaticJsonDocument<3072> doc;
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, payload, inputLength);

    if (error) {
        DBPrint("deserializeJson() failed: ");
        DBPrintln(error.c_str());
        return;
    }

    //== Payload ==================================================
    // https://arduinojson.org/v6/assistant/
    // {
    // "Brightness": 100,
    // "Mode": "Manual",
    // "Foreground": {
    //     "Mode": "TimeRainbow",
    //     "Slanted": true,
    //     "Color": 10145074
    // },
    // "Background": {
    //     "Mode": "ScrollingRainbow",
    //     "Color": 10145074
    // },
    // "Frame": {
    //     "Mode": "SolidColor",
    //     "Color": 10145074
    // },
    // "Recycling": {
    //     "Paper": {
    //     "Date": [
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]
    //     ],
    //     "Color": 10145074
    //     },
    // "Cardboard": {
    //     "Date": [
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]
    //     ],
    //     "Color": 10145074
    //     },
    // "Metal": {
    //     "Date": [
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],
    //         [  1, 2, 3, 4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]
    //     ],
    //     "Color": 10145074
    //     }
    // }
    // }

    //== Brightness ==================================================
    String Mode = doc["Mode"];  // "Manual"
    if ((Mode == "Auto") || (Mode == "Automatic")) {
        DBPrint("Mqtt Mode: ");
        DBPrintln(Mode);
        ClockInManualMode = false;
    } else if (Mode == "Manual") {
        DBPrint("Mqtt Mode: ");
        DBPrintln(Mode);
        ClockInManualMode = true;
    }

    int Brightness = doc["Brightness"];  // 1 - 100
    if (Brightness != 0) {
        DBPrint("Mqtt Brightness: ");
        DBPrint(Brightness);
        DBPrint(" % / ");
        DBPrint(map(Brightness, 1, 100, 0, 255));
        DBPrintln(" abs");
        pleddisp->setBrightness(map(Brightness, 1, 100, 0, 255));
    }

    if (ClockInManualMode) {
        //== Foreground ==================================================
        JsonObject Foreground = doc["Foreground"];
        String Foreground_Mode = Foreground["Mode"];      // "TimeRainbow"
        bool Foreground_Slanted = Foreground["Slanted"];  // true
        long Foreground_Color = Foreground["Color"];      // 10145074

        DBPrint("Mqtt setForegroundMode: ");
        DBPrint(Foreground_Mode);
        DBPrint(", Slanted: ");
        DBPrintln(Foreground_Slanted);
        if (Foreground_Mode == "None") {
            pleddisp->setForegroundMode(PLedDisp::ModeFG::None, Foreground_Slanted);
        } else if (Foreground_Mode == "Time") {
            pleddisp->setForegroundMode(PLedDisp::ModeFG::Time, Foreground_Slanted);
        } else if (Foreground_Mode == "TimeRainbow") {
            pleddisp->setForegroundMode(PLedDisp::ModeFG::TimeRainbow, Foreground_Slanted);
        } else if (Foreground_Mode == "Cycle") {
            pleddisp->setForegroundMode(PLedDisp::ModeFG::Cycle, Foreground_Slanted);
        } else {
            DBPrintln("Foreground_Mode: Invalid");
        }

        if (Foreground_Color != 0) {
            DBPrint("Mqtt setForegroundColor: ");
            DBPrintln(Foreground_Color);
            pleddisp->setForegroundColor(Foreground_Color);
        }

        //== Backround ==================================================
        String Background_Mode = doc["Background"]["Mode"];  // "ScrollingRainbow"
        long Background_Color = doc["Background"]["Color"];  // 10145074

        DBPrint("Mqtt setBackgroundMode: ");
        DBPrintln(Background_Mode);
        if (Background_Mode == "None") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::None);
        } else if (Background_Mode == "SolidColor") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::SolidColor);
        } else if (Background_Mode == "ScrollingRainbow") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::ScrollingRainbow);
        } else if (Background_Mode == "Twinkle") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::Twinkle);
        } else if (Background_Mode == "Fireworks") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::Fireworks);
        } else if (Background_Mode == "Thunderstorm") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::Thunderstorm);
        } else if (Background_Mode == "Firepit") {
            pleddisp->setBackgroundMode(PLedDisp::ModeBG::Firepit);
        } else {
            DBPrintln("Background_Mode: Invalid");
        }

        if (Background_Color != 0) {
            DBPrint("Mqtt setBackgroundColor: ");
            DBPrintln(Background_Color);
            pleddisp->setBackgroundColor(Background_Color);
        }

        //== Frame ==================================================
        String Frame_Mode = doc["Frame"]["Mode"];  // "SolidColor"
        long Frame_Color = doc["Frame"]["Color"];  // 10145074

        DBPrint("Mqtt setFrameMode: ");
        DBPrintln(Frame_Mode);
        if (Frame_Mode == "None") {
            pleddisp->setFrameMode(PLedDisp::ModeFR::None);
        } else if (Frame_Mode == "SolidColor") {
            pleddisp->setFrameMode(PLedDisp::ModeFR::SolidColor);
        } else if (Frame_Mode == "Time") {
            pleddisp->setFrameMode(PLedDisp::ModeFR::Time);
        } else {
            DBPrintln("Frame_Mode: Invalid");
        }
        if (Frame_Color != 0) {
            DBPrint("Mqtt setFrameColor: ");
            DBPrintln(Frame_Color);
            pleddisp->setFrameColor(Frame_Color);
        }
    }

    //== Recycling ==================================================
    for (JsonPair Recycling_item : doc["Recycling"].as<JsonObject>()) {
        String Recycling_item_key = Recycling_item.key().c_str();  // "Paper", "Cardboard", "Metal"
        DBPrint("Mqtt Recycling_item: ");
        DBPrintln(Recycling_item_key);

        if (Recycling_item_key == "Paper") {
            if (Recycling_item.value()["Color"] != 0) {
                recyclingPaperColor = Recycling_item.value()["Color"];
            }
            JsonArray Recycling_item_value_Date_0 = Recycling_item.value()["Date"][0];
            JsonArray Recycling_item_value_Date_1 = Recycling_item.value()["Date"][1];
            for (int ii = 0; ii < (sizeof(recyclingPaperDates) / sizeof(recyclingPaperDates[0])); ii++) {
                recyclingPaperDates[ii][0] = Recycling_item_value_Date_0[ii];
                recyclingPaperDates[ii][1] = Recycling_item_value_Date_1[ii];
                DBPrint(recyclingPaperDates[ii][0]);
                DBPrint("/");
                DBPrintln(recyclingPaperDates[ii][1]);
            }
        } else if (Recycling_item_key == "Cardboard") {
            if (Recycling_item.value()["Color"] != 0) {
                recyclingCardboardColor = Recycling_item.value()["Color"];
            }
            JsonArray Recycling_item_value_Date_0 = Recycling_item.value()["Date"][0];
            JsonArray Recycling_item_value_Date_1 = Recycling_item.value()["Date"][1];
            for (int ii = 0; ii < (sizeof(recyclingCardboardDates) / sizeof(recyclingCardboardDates[0])); ii++) {
                recyclingCardboardDates[ii][0] = Recycling_item_value_Date_0[ii];
                recyclingCardboardDates[ii][1] = Recycling_item_value_Date_1[ii];
                DBPrint(recyclingCardboardDates[ii][0]);
                DBPrint("/");
                DBPrintln(recyclingCardboardDates[ii][1]);
            }
        } else if (Recycling_item_key == "Metal") {
            if (Recycling_item.value()["Color"] != 0) {
                recyclingMetalColor = Recycling_item.value()["Color"];
            }
            JsonArray Recycling_item_value_Date_0 = Recycling_item.value()["Date"][0];
            JsonArray Recycling_item_value_Date_1 = Recycling_item.value()["Date"][1];
            for (int ii = 0; ii < (sizeof(recyclingMetalDates) / sizeof(recyclingMetalDates[0])); ii++) {
                recyclingMetalDates[ii][0] = Recycling_item_value_Date_0[ii];
                recyclingMetalDates[ii][1] = Recycling_item_value_Date_1[ii];
                DBPrint(recyclingMetalDates[ii][0]);
                DBPrint("/");
                DBPrintln(recyclingMetalDates[ii][1]);
            }
        }
    }
}

bool reconnect() {
    if (mqttClient.connect("ESPClient", MQTT_USERNAME, MQTT_PASSWORD)) {
        // Once connected, publish an announcement...
        mqttClient.publish("home/hallway/clock", "Esp32-Clock connected");
        // ... and resubscribe
        mqttClient.subscribe("home/hallway/clock");
    }
    return mqttClient.connected();
}