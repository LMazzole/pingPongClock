/**
 * @file LogConfiguration.h
 * @brief
 *
 * @author Luca Mazzoleni (luca_mazzoleni92@hotmail.com)
 *
 * @version 1.0 - Description - {author} - {date}
 *
 * @date 2022-01-08
 * @copyright Copyright (c) 2022
 *
 */
#pragma once

#define DEBUGMODE ;

#ifdef DEBUGMODE
#define DBPrint(x)       \
    if (Serial) {        \
        Serial.print(x); \
    }
#define DBPrintln(x)       \
    if (Serial) {          \
        Serial.println(x); \
    }
#else
#define DBPrint(x)
#define DBPrintln(x)
#endif