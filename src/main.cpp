/*
 * Copyright 2018 Bolukan
 * MIT License, https://opensource.org/licenses/MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 *  'Sensor-framework'
 *  Combines: BME280, WiFi, NTP, MQTT
 */

/*
 *  Libraries:
 *  Time, Time by Michael Margolis version 1.5.0
 *    https://github.com/PaulStoffregen/Time
 *  NTPClient, NTPClientLib by German Martin version 2.5.0
 *    https://github.com/gmag11/NtpClient
 */

#include <ESP8266WiFi.h>
#include "WiFiConfig.h"    // WiFi Credentials
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <Wire.h>
#include <BME280I2C.h>
#include <Syslog.h>

// ESP8266WiFi
#ifndef WIFI_CONFIG_H
 #define WIFI_SSID     "WIFI_SSID"
 #define WIFI_PASSWD   "WIFI_PASSWD"
#endif

bool wifiFirstConnected = false;

// NTPClient
String NTPServerName = "pool.ntp.org";
int8_t timeZone = 1;
int8_t minutesTimeZone = 0;
// global
boolean syncEventTriggered = false; // True if a time event has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

/*
 *  Syslog
 */
#define SYSLOG_SERVER "192.168.1.215"
#define SYSLOG_PORT 516

// This device info
#define DEVICE_HOSTNAME "D1-mini"
#define APP_NAME "BME280"

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpClient;

// Create a new syslog instance with LOG_KERN facility
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN, SYSLOG_PROTO_BSD);

/*
 * BME280
 */

BME280I2C bme;

/*
 * WiFi
 */

// Connected
void onSTAConnected (WiFiEventStationModeConnected ipInfo) {
    Serial.printf ("Connected to %s\r\n", ipInfo.ssid.c_str ());
}

// Start NTP only after IP network is connected
void onSTAGotIP (WiFiEventStationModeGotIP ipInfo) {
    Serial.printf ("Connected: %s\r\n", WiFi.status() == WL_CONNECTED ? "yes" : "no");
    Serial.printf ("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
    wifiFirstConnected = true;
}

// Manage network disconnection
void onSTADisconnected (WiFiEventStationModeDisconnected event_info) {
    Serial.printf ("Disconnected from SSID: %s\n", event_info.ssid.c_str());
    Serial.printf ("Reason: %d\n", event_info.reason);
    //NTP.stop(); // NTP sync can be disabled to avoid sync errors
}

/*
 * NTPClient
 */

void processSyncEvent(NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
        Serial.print("Time Sync error: ");
        if (ntpEvent == noResponse)
            Serial.println("NTP server not reachable");
        else if (ntpEvent == invalidAddress)
            Serial.println("Invalid NTP server address");
    } else {
        Serial.print("Got NTP time: ");
        Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
    }
}

void printBME280Data(Stream* client)
{
   float temp(NAN), hum(NAN), pres(NAN);
   char TempC[8], Humidity[8], Pressure[8];

   BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
   BME280::PresUnit presUnit(BME280::PresUnit_hPa);

   bme.read(pres, temp, hum, tempUnit, presUnit);
   dtostrf(temp, 4, 1, TempC);
   dtostrf(hum, 4, 1, Humidity);
   dtostrf(pres, 5, 1, Pressure);

   syslog.logf(LOG_INFO, "Temp %s, Humi %s, Pres %s        ", TempC, Humidity, Pressure);
   client->print(F("Temperature: "));
   client->print(TempC);
   client->print(F(" °C\t\tHumidity: "));
   client->print(Humidity);
   client->print(F("% RH\t\tPressure: "));
   client->print(Pressure);
   client->println(F(" Pa"));
}

void setup () {
    static WiFiEventHandler e1, e2, e3;

    Serial.begin(115200);
    Serial.println();

    // WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);

    // BME280
    Wire.begin();
    Serial.println("Starting BME280");
    while (!bme.begin()) {
      Serial.println("Could not find BME280 sensor");
      delay(1000);
    }

    switch(bme.chipModel())
    {
       case BME280::ChipModel_BME280:
         Serial.println("Found BME280 sensor! Success.");
         break;
       case BME280::ChipModel_BMP280:
         Serial.println("Found BMP280 sensor! No Humidity available.");
         break;
       default:
         Serial.println("Found UNKNOWN sensor! Error!");
    }

    syslog.log(LOG_INFO, "Device setup");

    // events
    NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
        ntpEvent = event;
        syncEventTriggered = true;
    });

    e1 = WiFi.onStationModeGotIP(onSTAGotIP);// As soon WiFi is connected, start NTP Client
    e2 = WiFi.onStationModeConnected(onSTAConnected);
    e3 = WiFi.onStationModeDisconnected(onSTADisconnected);
}

void loop () {
    // static int i = 0;
    static int last = 0;

    if (wifiFirstConnected) {
        wifiFirstConnected = false;
        NTP.begin (NTPServerName, timeZone, true, minutesTimeZone);
        NTP.setInterval (60*15);
    }

    if (syncEventTriggered) {
       syncEventTriggered = false;
        processSyncEvent (ntpEvent);
    }

    if ((millis () - last) > 60000) {

        printBME280Data(&Serial);

        //Serial.println(millis() - last);
        last = millis ();
        /*
        Serial.print (i); Serial.print (" ");
        Serial.print (NTP.getTimeDateString ()); Serial.print (" ");
        Serial.print ("WiFi is ");
        Serial.print (WiFi.isConnected () ? "connected" : "not connected"); Serial.print (". ");
        Serial.print ("Uptime: ");
        Serial.print (NTP.getUptimeString ()); Serial.print (" since ");
        Serial.println (NTP.getTimeDateString (NTP.getFirstSync ()).c_str ());

        i++;
        */
    }
    delay (0);
}
