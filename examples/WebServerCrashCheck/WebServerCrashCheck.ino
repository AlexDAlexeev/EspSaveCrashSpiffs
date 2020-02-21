/*
  Example application to show how to save crash information
  to ESP8266's flash using EspSaveCrash library
  Please check repository below for details

  Repository: https://github.com/brainelectronics/EspSaveCrashSpiffs
  File: WebServerCrashCheck.ino
  Revision: 0.1.1
  Date: 09-Jan-2020
  Author: brainelectronics

  based on code of
  Repository: https://github.com/krzychb/EspSaveCrash
  File: RemoteCrashCheck.ino
  Revision: 1.0.2
  Date: 17-Aug-2016
  Author: krzychb at gazeta.pl

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

extern "C" {
#include "user_interface.h"
}

// include custom lib for this example
#include "EspSaveCrashSpiffs.h"

// include Arduino libs
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// WiFi ssid and password defined in wifiConfig.h
// file might be tracked with gitignore
#include "wifiConfig.h"

// use the default file name defined in EspSaveCrashSpiffs.h
ESP8266WebServer server(80);

ESP8266WiFiMulti WiFiMulti;

// flag for setting up the server with WiFiMulti.addAP
uint8_t performOnce = 0;

void setup(void)
{
  Serial.begin(115200);
  Serial.println("\nWebServerCrashCheck.ino");

  Serial.printf("Connecting to %s ", ssid);

  // WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  // credentials will be added to onboard WiFi settings
  WiFiMulti.addAP(ssid, password);
  // // you have to wait until WiFi.status() == WL_CONNECTED
  // // before starting MDNS and the webserver
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   delay(500);
  //   Serial.print(".");
  // }
  // Serial.println(" connected");

  // create AccessPoint with custom network name and password
  WiFi.softAP(accessPointSsid, accessPointPassword);
  Serial.printf("AccessPoint created. Connect to '%s' network and open '%s' in a web browser\n", accessPointSsid, WiFi.softAPIP().toString().c_str());

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/list", handleListFiles);
  server.on("/file", handleFilePath);
  server.onNotFound(handleNotFound);

  // start the http server
  server.begin();

  Serial.println("\nPress a key + <enter>");
  Serial.println("0 : attempt to divide by zero");
  Serial.println("e : attempt to read through a pointer to no object");
  Serial.println("p : print crash information");
  Serial.println("b : store crash information to buffer and print buffer");
  Serial.println("i : print IP address");
}

void loop(void)
{
  server.handleClient();
  MDNS.update();

  // if a WiFi connection is successfully established
  if (WiFi.status() == WL_CONNECTED)
  {
    // if the following action has not been perfomed yet
    if (performOnce == 0)
    {
      // change the flag
      performOnce = 1;

      Serial.printf("Connected to '%s' as '%s' \n", ssid, WiFi.localIP().toString().c_str());

      // try to start MDNS service
      if (MDNS.begin("esp8266"))
      {
        MDNS.addService("http", "tcp", 80);

        Serial.println("MDNS service started. This device is also available as 'esp8266.local' in a web browser");
      }
      else
      {
        Serial.println("MDNS service failed to start.");
      }
    }
  }

  // read the keyboard
  if (Serial.available() > 0)
  {
    char inChar = Serial.read();

    switch (inChar)
    {
      case '0':
        Serial.println("Attempting to divide by zero ...");
        int result, zero;
        zero = 0;
        result = 1 / zero;
        Serial.print("Result = ");
        Serial.println(result);
        break;
      case 'e':
        Serial.println("Attempting to read through a pointer to no object ...");
        int* nullPointer;
        nullPointer = NULL;
        // null pointer dereference - read
        // attempt to read a value through a null pointer
        Serial.print(*nullPointer);
        break;
      case 'p':
        printLogToSerial();
        break;
      case 'b':
        printLogToBuffer();
        break;
      case 'i':
        // print the IP address of the http server
        if (performOnce == 1)
        {
          Serial.printf("Connected to '%s' with IP address: %s\n", ssid, WiFi.localIP().toString().c_str());
        }
        else
        {
          Serial.printf("Connection to network '%s' not yet established\n", ssid);
        }
        Serial.printf("Connect to AccessPoint '%s' at IP address: %s\n", accessPointSsid, WiFi.softAPIP().toString().c_str());
        break;
      default:
        break;
    }
  }
}


/**
 * @brief      Put the latest log file content to buffer and print it.
 *
 * This will only print the log file if there is enought free RAM to allocate.
 */
void printLogToBuffer()
{
    // get the last filename
    const String& _lastCrashFileName = SaveCrashSpiffs.getLastCrashLogFilePath();
    Serial.print("Name of last log file: '");
    Serial.print(_lastCrashFileName);
    Serial.println("'");

    // open the file in reading mode
    File theLogFile = SPIFFS.open(_lastCrashFileName, "r");

    // get the size of the file
    size_t _crashFileSize = theLogFile.size();
    theLogFile.close();

    // get free heap/RAM of the system
    uint32_t _ulFreeHeap = system_get_free_heap_size();

    // check if file size is smaller than available RAM
    if (_ulFreeHeap > _crashFileSize) {
        // create buffer for file content with the size of the file+1
        char* _crashFileContent;
        _crashFileContent = reinterpret_cast<char*>(malloc(_crashFileSize + 1));

        // read the file content to the buffer
        SaveCrashSpiffs.readFile(_lastCrashFileName.c_str(), _crashFileContent, _crashFileSize + 1);

        Serial.println("--- BEGIN of crash file ---");
        Serial.print(_crashFileContent);
        Serial.println("--- END of crash file ---");

        // free the allocated space
        free(_crashFileContent);
    } else {
        Serial.print("Error reading file '");
        Serial.print(_lastCrashFileName);
        Serial.print("' to buffer.");
        Serial.print(_ulFreeHeap);
        Serial.print(" byte of RAM is not enough to read ");
        Serial.print(_crashFileSize);
        Serial.println(" byte of file content");
    }
}

/**
 * @brief      Print the latest log file to serial.
 */
void printLogToSerial()
{
    // get the last filename
    const String& _lastCrashFileName = SaveCrashSpiffs.getLastCrashLogFilePath();

    Serial.print("Name of last log file: '");
    Serial.print(_lastCrashFileName);
    Serial.println("'");

    Serial.println("--- BEGIN of crash file ---");
    SaveCrashSpiffs.print(_lastCrashFileName.c_str());
    Serial.println("--- END of crash file ---");
}

/**
 * @brief      Handle access on root page
 */
void handleRoot()
{
  server.send(200, "text/html", "<html><body>Hello from ESP<br><a href='/log' target='_blank'>Latest crash Log</a><br><a href='/list' target='_blank'>List all files in root directory</a><br><a href='/file?path=/crashLog-2.log' target='_blank'>Crash Log file #2</a><br></body></html>");
}

void serverSendContent(ESP8266WebServer& server, const String& fileName)
{
    // open the file in reading mode
    File theLogFile = SPIFFS.open(fileName, "r");

    // get the size of the file
    size_t _crashFileSize = theLogFile.size();
    theLogFile.close();

    // get free heap/RAM of the system
    uint32_t _ulFreeHeap = system_get_free_heap_size();

    // check if file size is smaller than available RAM
    if (_ulFreeHeap > (_crashFileSize + 1)) {
        // create buffer for file content with the size of the file+1
        char* _crashFileContent;
        _crashFileContent = reinterpret_cast<char*>(malloc(_crashFileSize + 1));

        // read the file content to the buffer
        SaveCrashSpiffs.readFile(fileName.c_str(), _crashFileContent, _crashFileSize + 1);
        server.send(200, "text/plain", _crashFileContent);

        // free the allocated space
        free(_crashFileContent);
    } else {
        String _errorContent = "507: Insufficient Storage. Error reading file '";
        _errorContent += fileName + "' to buffer.";
        _errorContent += String(_ulFreeHeap) + " byte of RAM is not enough to read " + String(_crashFileSize) + " byte of file content";
        server.send(507, "text/plain", _errorContent);
    }
}
/**
 * @brief      Handle access on log page
 */
void handleLog()
{
    serverSendContent(server, SaveCrashSpiffs.getLastCrashLogFilePath());
}

/**
 * @brief      Handle given filepath with URL
 */
void handleFilePath()
{
  // if parameter is empty
  if (server.arg("path") == "")
  {
    server.send(404, "text/plain", "Path argument not found");
  }
  else
  {
      serverSendContent(server, server.arg("path"));
  }
     
}

/**
 * @brief      Handle access to filelist webpage
 *
 * Show a list of files with their size on the page
 */
void handleListFiles()
{
    String pageContent = "<html><body>List of crash logs<br>";
    SaveCrashSpiffs.iterateCrashLogFiles([&](uint32_t fileNumber, const char* fileName) {
      const String fn = fileName;
        pageContent += "<a href='/file?path=" + fn + "' target='_blank'>"+fn+"</a><br>";
    });
    pageContent += "</body></html>";
    // serve the page
    server.send(200, "text/html", pageContent);
}

/**
 * @brief      Provide 404 Not found page
 */
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}
