/*
  This in an Arduino library to save exception details
  and stack trace to flash in case of ESP8266 crash.
  Please check repository below for details

  Repository: https://github.com/AlexDAlexeev/EspSaveCrashSpiffs
  File: EspSaveCrashSpiffs.h
  Revision: 0.1.0
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

#include "EspSaveCrashSpiffs.h"

// Global instance EspSaveCrashSpiffs
EspSaveCrashSpiffs SaveCrashSpiffs;

/**
 * This function is called automatically if ESP8266 suffers an exception
 * It should be kept quick / consise to be able to execute before hardware wdt may kick in
 *
 * Without writing to SPIFFS this function take 2-3ms
 * Writing to flash only takes 10-11ms.
 * This complete function should be finised in 15-20ms
 */
extern "C" void custom_crash_callback(struct rst_info* rst_info, uint32_t stack, uint32_t stack_end)
{
    FS& fs = SaveCrashSpiffs._fs;

    uint32_t crashTime = millis();

    // open the file in appending mode
    File fileCrashFile = fs.open(SaveCrashSpiffs.getCrashLogFilePath(), "a");

    // if the file does not yet exist
    if (!fileCrashFile) {
        // open the file in write mode
        fileCrashFile = fs.open(SaveCrashSpiffs.getCrashLogFilePath(), "w");
    }

    if (!fileCrashFile) {
        return;
    }
    // if the file is (now) a valid file

    // maximum tmpBuffer size needed is 83, so 100 should be enough
    char tmpBuffer[100];

    // max. 65 chars of Crash time, reason, exception
    sprintf_P(tmpBuffer, PSTR("Crashed at %d ms\nRestart reason: %d\nException cause: %d\n"), crashTime, rst_info->reason, rst_info->exccause);
    fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

    // 83 chars of epc1, epc2, epc3, excvaddr, depc info + 13 chars of >stack>
    sprintf_P(tmpBuffer, PSTR("epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x\n>>>stack>>>\n"),
        rst_info->epc1, rst_info->epc2, rst_info->epc3, rst_info->excvaddr, rst_info->depc);
    fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

    uint16_t stackLength = stack_end - stack;
    uint32_t stackTrace;

    // collect stack trace
    // one loop contains 45 chars of stack address and its content
    // e.g. "3fffffb0: feefeffe feefeffe 3ffe8508 40100459"
    for (uint16_t i = 0; i < stackLength; i += 0x10) {

        sprintf_P(tmpBuffer, PSTR("%08x: "), stack + i);
        fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));

        for (int j = 0; j < 4; j++) {
            uint32_t* byteptr = (uint32_t*)(stack + i + j * 4);
            stackTrace = *byteptr;

            sprintf_P(tmpBuffer, PSTR("%08x "), stackTrace);
            fileCrashFile.write(tmpBuffer, strlen(tmpBuffer));
        }
        fileCrashFile.write('\n');
    }
    fileCrashFile.write("<<<stack<<<\n\n");
    fileCrashFile.close();
}

/**
 * @brief      Constructs a new instance.
 *
 * check whether the crash file is not empty.
 * if the file has content
 *  - check weather enough space is available
 *  - find the next filename (based on the crash file name and extension)
 *  - rename the file to the next free filename
 * if the file is empty, continue without any action
 */
EspSaveCrashSpiffs::EspSaveCrashSpiffs(const String& directory, const String& prefix, const String& suffix, FS& fs)
    : _fs(fs)
    , _fileDirectory(directory.isEmpty() ? "/" : directory)
    , _filePrefix(prefix.isEmpty() ? DEFAULT_CRASHFILEPREFIX : prefix)
    , _fileSuffix(suffix.isEmpty() ? DEFAULT_CRASHFILESUFFIX : suffix)
{
    fs.begin();
    // Default file path
    setLogFileNameParams(_fileDirectory, _filePrefix, _fileSuffix);
}

void EspSaveCrashSpiffs::setLogFileNameParams(const char* directory, const char* prefix, const char* suffix)
{
    _fileDirectory = directory;
    _filePrefix = prefix;
    _fileSuffix = suffix;
    _renewLogFiles();
}

/**
 * @brief      Gets the current and latest log files names
 */
void EspSaveCrashSpiffs::_renewLogFiles()
{
    char nextCrashLogFile[255];
    _getNextFileName(nextCrashLogFile, true);
    // Log file to write crash if any
    _crashLogFile = nextCrashLogFile;
    // Latest crash file
    _getNextFileName(nextCrashLogFile, false);
    _lastLogFile = nextCrashLogFile;
}

/**
 * @brief      If file name matched current settings returns file number, else returns 0
 * @param[in]  fileName   File name for test
 * @retval     0   File does not match
 * @retval     >0   File number
**/
uint32_t EspSaveCrashSpiffs::_getNumberOfNameMatch(const String& fileName)
{
    if (!fileName.startsWith(_filePrefix)) {
        // Not a log file
        return 0;
    }
    if (!fileName.endsWith(_fileSuffix)) {
        // Not a log file
        return 0;
    }
    const auto incrementalPart = fileName.substring(_filePrefix.length(), fileName.length() - _fileSuffix.length());
    const int fileNumber = strtol(incrementalPart.c_str(), nullptr, 10);
    return fileNumber;
}

/**
 * @brief      Find the next or most recent file name.
 *
 * Crawl the directory for files matching the pattern and the extension.
 *
 * @param[out] nextFileName    The next file name
 * @param[in]  directoryName   Directory for files
 * @param[in]  filePrefix      The file prefix
 * @param[in]  fileExtension   The file suffix (extension)
 * @param[in]  findNextName    Use next or last file
 *
 * Example usage to find the most recent file:
 * @code
 *    char fileName[100];
 
 *    // find the most recent file starting with 'crashLog-'
 *    // and ending with '.log' in the root directory '/'
 *    // fileName will be e.g. 'crashLog-123.log'
 *    // if the most recent one was called 'crashLog-123.log'
 *    _getNextFileName(fileName, "/", "crashLog-", ".log");
 * @endcode
 *
 * Example usage to find the next file:
 * @code
 *    // allocate some space for the file name
 *    char* fileName = (char*)calloc(100, sizeof(char));
 *
 *    // find the next filename based on starting with 'crashLog'
 *    // and ending with 'log' in the root directory '/'
 *    // fileName will be e.g. 'crashLog-124.log'
 *    // if the most recent one was called 'crashLog-123.log'
 *    _find_file_name(1, fileName, "/", "crashLog", "log");
 * @endcode
 */
    void EspSaveCrashSpiffs::_getNextFileName(char* nextFileName, bool findNextName /* = true*/)
{
    nextFileName[0] = '\0';

    Dir crashLogDirectory = _fs.openDir(_fileDirectory);
    uint32_t currentLogNumber = 0;
    // Enumerate files in dir
    while (crashLogDirectory.next()) {
        if (!crashLogDirectory.isFile()) {
            continue;
        }
        int namePos = crashLogDirectory.fileName().lastIndexOf('/');
        const auto fileName = crashLogDirectory.fileName().substring(namePos >= 0 ? namePos + 1 : 0);

        const auto fileNumber = _getNumberOfNameMatch(fileName);
        if (fileNumber == 0) {
            // Invalid number
            continue;
        }
        if (currentLogNumber < fileNumber) {
            currentLogNumber = fileNumber;
            strcpy(nextFileName, crashLogDirectory.fileName().c_str());
        }
    }
    if (!findNextName && nextFileName[0] != '\0') {
        // Last file name found, return
        return;
    }
    // Assume that currentLogNumber is last file number found
    // Construct new file path
    _makeFileName(nextFileName, currentLogNumber + 1);
}

void EspSaveCrashSpiffs::_makeFileName(char* fileName, uint32_t fileNumber)
{
    strcpy(fileName, _fileDirectory.c_str());
    strcat(fileName, _filePrefix.c_str());
    strcat(fileName, String(fileNumber).c_str());
    strcat(fileName, _fileSuffix.c_str());
}

/**
 * @brief      Removes a file.
 *
 * If the given number is zero, the current log file will be removed.
 * Otherwise the file at that index.
 *
 * @param[in]  fileNumber  The file number
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
bool EspSaveCrashSpiffs::removeFile(uint32_t fileNumber)
{
    char fileName[255];
    // Construct new file path
    if (fileNumber == 0) {
        strcpy(fileName, _lastLogFile.c_str());
    } else {
        _makeFileName(fileName, fileNumber);
    }

    // if there is a filenumber given, remove this
    if (_fs.exists(fileName)) {
        bool res =  _fs.remove(fileName);
        _renewLogFiles();
        return res;
    }
    return false;
}

/**
 * @brief      Check file operations.
 *
 * This function checks if:
 *  - the file system can be accessed
 *  - the file exists
 *  - the file can be written or read
 * @param[in]  theFileName  The file name
 * @param[in]  openMode     The open mode "r", "w", "a"...
 *
 * @retval     True   Success
 * @retval     False  Failed
 */
bool EspSaveCrashSpiffs::_checkFile(const char* theFileName, const char* openMode)
{
    // if starting the SPIFFS failed
    if (!_fs.begin()) {
        return false;
    }

    // if the file does not exist
    if (!_fs.exists(theFileName)) {
        return false;
    }

    // if reading or writing operations should also be checked
    if (openMode) {
        File theFile = _fs.open(theFileName, openMode);

        // if opening the file failed
        if (!theFile) {
            return false;
        }

        theFile.close();
    }

    return true;
}

/**
 * @brief      Reads a file to buffer.
 *
 * @param[in]  fileName    The file name
 * @param      size        The size
 * @param      buffer      The user buffer to store the content of the file
 * @param      bufferSize  Size of the user buffer to store the content of the file
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::readFile(const char* fileName, char* buffer, size_t bufferSize)
{
    buffer[0] = '\0';
    // check if SPIFFS is working and file exists.
    // NULL parameter as checking reading or writing is not neccessary here
    if (!_checkFile(fileName, "r")) {
        // exit on file error
        return false;
    }

    File theFile = _fs.open(fileName, "r");
    theFile.readBytes(buffer, max(theFile.size(), bufferSize));
    theFile.close();

    return true;
}

/**
 * @brief      Reads a file to buffer.
 *
 * @param[in]  fileNumber  Number of log file
 * @param      size        The size
 * @param      buffer      The user buffer to store the content of the file
 * @param      bufferSize  Size of the user buffer to store the content of the file
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::readFile(uint32_t fileNumber, char* buffer, size_t bufferSize)
{
    char fileName[255];
    // Construct new file path
    if (fileNumber == 0) {
        strcpy(fileName, _lastLogFile.c_str());
    } else {
        _makeFileName(fileName, fileNumber);
    }
    return readFile(fileName, buffer, bufferSize);
}

/**
 * @brief      Print the file content to outputDev
 *
 * @param[in]  fileName   The file name
 * @param      outputDev  The output dev
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::print(const char* fileName, Print& outputDev)
{
    // check if SPIFFS is working and file exists.
    // NULL parameter as checking reading or writing is not neccessary here
    if (!_checkFile(fileName, "r")) {
        // exit on file error
        return false;
    }

    File theFile = _fs.open(fileName, "r");

    while (theFile.available()) {
        outputDev.write(theFile.read());
    }

    theFile.close();

    return true;
}

/**
 * @brief      Print the file content to outputDev
 *
 * @param[in]  fileNumber  Number of log file
 * @param      outputDev   The output dev
 *
 * @return     Sucess or error accessing the file
 */
bool EspSaveCrashSpiffs::print(uint32_t fileNumber, Print& outputDev)
{
    char fileName[255];
    // Construct new file path
    if (fileNumber == 0) {
        strcpy(fileName, _lastLogFile.c_str());
    } else {
        _makeFileName(fileName, fileNumber);
    }
    return print(fileName, outputDev);
}

/**
 * @brief      Count files matching the pattern
 *
 * @return     Number of files matching the pattern
 */
uint32_t EspSaveCrashSpiffs::count()
{
    Dir crashLogDirectory = _fs.openDir(_fileDirectory);
    uint32_t filesCount = 0;
    // Enumerate files in dir
    while (crashLogDirectory.next()) {
        if (!crashLogDirectory.isFile()) {
            continue;
        }
        const int namePos = crashLogDirectory.fileName().lastIndexOf('/');
        const auto fileName = crashLogDirectory.fileName().substring(namePos >= 0 ? namePos + 1 : 0);
        if(_getNumberOfNameMatch(fileName) != 0) {
            filesCount++;
        }
    }
    return filesCount;
}

/**
 * @brief      Count files matching the pattern
 *
 * @param      callback function called for each crash log file
 */

void EspSaveCrashSpiffs::iterateCrashLogFiles(OnCrashLogFileFound callback)
{
    Dir crashLogDirectory = _fs.openDir(_fileDirectory);
    // Enumerate files in dir
    while (crashLogDirectory.next()) {
        if (!crashLogDirectory.isFile()) {
            continue;
        }
        const int namePos = crashLogDirectory.fileName().lastIndexOf('/');
        const auto fileName = crashLogDirectory.fileName().substring(namePos >= 0 ? namePos + 1 : 0);
        const auto fileNumber = _getNumberOfNameMatch(fileName);
        if(fileNumber==0){
            continue;
        }
        callback(fileNumber, crashLogDirectory.fileName().c_str());
    }
}

