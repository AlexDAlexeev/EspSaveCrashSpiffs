/*
  This in an Arduino library to save exception details
  and stack trace to flash in case of ESP8266 crash.
  Please check repository below for details

  Repository: https://github.com/AlexDAlexeev/EspSaveCrashFs
  File: EspSaveCrashFs.h
  Revision: 0.1.1
  Date: 20-Feb-2020
  Author: Alex.D.Alexeev

  based on code of

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: EspSaveCrashSpiffs.h
  Revision: 0.1.0
  Date: 04-Jan-2020
  Author: brainelectronics

  based on code of
  Date: 18-Aug-2016
  Author: krzychb
  Repository: https://github.com/krzychb/EspSaveCrash

  inspired by code of:
  Date: 28-Dec-2018
  Author: popok75
  Repository: https://github.com/esp8266/Arduino/issues/1152

  Copyright (c) 2020 brainelectronics. All rights reserved.

  This application is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This application is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _EspSaveCrashFs_H_
#define _EspSaveCrashFs_H_

#pragma once

#include "Arduino.h"
#include "user_interface.h"

#ifndef ESPSAVECRASH_SPIFFS
#include <LittleFS.h>
#else
#include "FS.h"
#endif

// define the usage of SPIFFS crash log before anything else
// #define SPIFFS_CRASH_LOG  1
// #define EEPROM_CRASH_LOG

/**
 * Structure of the single crash data set
 *
 *  1. Crash time
 *  2. Restart reason
 *  3. Exception cause
 *  4. epc1
 *  5. epc2
 *  6. epc3
 *  7. excvaddr
 *  8. depc
 *  9. adress of stack start
 * 10. adress of stack end
 * 11. stack trace bytes
 *     ...
 */

#ifndef DEFAULT_CRASHFILEPREFIX
#define DEFAULT_CRASHFILEPREFIX PSTR("crashLog-")
#endif

#ifndef DEFAULT_CRASHFILESUFFIX
#define DEFAULT_CRASHFILESUFFIX PSTR(".log")
#endif

class EspSaveCrashFs {
public:
    typedef std::function<void(uint32_t fileNumber, const char* fileName)> OnCrashLogFileFound;

#ifndef ESPSAVECRASH_SPIFFS
    EspSaveCrashFs(const String& directory = "", const String& prefix = "", const String& suffix = "", FS& fs = LittleFS);
#else
    EspSaveCrashFs(const String& directory = "", const String& prefix = "", const String& suffix = "", FS& fs = SPIFFS);
#endif

    const String& getLogFileDirectory() const { return _fileDirectory; }
    const String& getLogFilePrefix() const { return _filePrefix; }
    const String& getLogFileSuffix() const { return _fileSuffix; }

    void setLogFileNameParams(const char* directory, const char* prefix, const char* suffix);
    void setLogFileNameParams(const String& directory, const String& prefix, const String& suffix) { setLogFileNameParams(directory.c_str(), prefix.c_str(), suffix.c_str()); }

    const String& getCrashLogFilePath() const { return _crashLogFile; }
    const String& getLastCrashLogFilePath() const { return _lastLogFile; }

    bool removeFile(uint32_t fileNumber);
    bool readFile(uint32_t fileNumber, char* buffer, size_t bufferSize);
    bool print(uint32_t fileNumber, Print& outDevice = Serial);

    uint32_t count();
    void iterateCrashLogFiles(OnCrashLogFileFound callback);

    bool readFile(const char* fileName, char* buffer, size_t bufferSize);
    bool print(const char* fileName, Print& outDevice = Serial);

    FS& _fs;
private:
    String _fileDirectory;
    String _filePrefix;
    String _fileSuffix;
    String _crashLogFile;
    String _lastLogFile;

    void _renewLogFiles();
    void _getNextFileName(char* nextFileName, bool findNextName = true);
    void _makeFileName(char* fileName, uint32_t fileNumber);
    bool _checkFile(const char* theFileName, const char* openMode);
    uint32_t _getNumberOfNameMatch(const String& fileName);
};

extern EspSaveCrashFs SaveCrashSpiffs;

#endif
