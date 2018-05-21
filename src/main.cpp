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

#include <ESP8266WiFi.h>   // WiFi
#include "WiFiConfig.h"   // WiFi Credentials
#include <TimeLib.h>
#include <NtpClientLib.h>

// ESP8266WiFi
#ifndef WIFI_CONFIG_H
 #define WIFI_SSID     "WIFI_SSID"
 #define WIFI_PASSWD   "WIFI_PASSWD"
#endif

bool wifiFirstConnected = false;

// NTPClient
int8_t timeZone = 1;
int8_t minutesTimeZone = 0;
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

/*
 * WiFi
 */

// Connected
void onSTAConnected (WiFiEventStationModeConnected ipInfo) {
    Serial.printf ("Connected to %s\r\n", ipInfo.ssid.c_str ());
}

// Start NTP only after IP network is connected
void onSTAGotIP (WiFiEventStationModeGotIP ipInfo) {
    Serial.printf ("Got IP: %s\r\n", ipInfo.ip.toString ().c_str ());
    Serial.printf ("Connected: %s\r\n", WiFi.status () == WL_CONNECTED ? "yes" : "no");
    wifiFirstConnected = true;
}

// Manage network disconnection
void onSTADisconnected (WiFiEventStationModeDisconnected event_info) {
    Serial.printf ("Disconnected from SSID: %s\n", event_info.ssid.c_str ());
    Serial.printf ("Reason: %d\n", event_info.reason);
    //NTP.stop(); // NTP sync can be disabled to avoid sync errors
}

/*
 * NTPClient
 */

void processSyncEvent (NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
        Serial.print ("Time Sync error: ");
        if (ntpEvent == noResponse)
            Serial.println ("NTP server not reachable");
        else if (ntpEvent == invalidAddress)
            Serial.println ("Invalid NTP server address");
    } else {
        Serial.print ("Got NTP time: ");
        Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
    }
}

void setup () {
    static WiFiEventHandler e1, e2, e3;

    Serial.begin (115200);
    Serial.println ();
    WiFi.mode (WIFI_STA);
    WiFi.begin (WIFI_SSID, WIFI_PASSWD);

    NTP.onNTPSyncEvent ([](NTPSyncEvent_t event) {
        ntpEvent = event;
        syncEventTriggered = true;
    });

    e1 = WiFi.onStationModeGotIP (onSTAGotIP);// As soon WiFi is connected, start NTP Client
    e2 = WiFi.onStationModeDisconnected (onSTADisconnected);
    e3 = WiFi.onStationModeConnected (onSTAConnected);
}

void loop () {
    static int i = 0;
    static int last = 0;

    if (wifiFirstConnected) {
        wifiFirstConnected = false;
        NTP.begin ("pool.ntp.org", timeZone, true, minutesTimeZone);
        NTP.setInterval (60*30);
    }

    if (syncEventTriggered) {
        processSyncEvent (ntpEvent);
        syncEventTriggered = false;
    }

    if ((millis () - last) > 10000) {
        //Serial.println(millis() - last);
        last = millis ();
        Serial.print (i); Serial.print (" ");
        Serial.print (NTP.getTimeDateString ()); Serial.print (" ");
        Serial.print (NTP.isSummerTime () ? "Summer Time. " : "Winter Time. ");
        Serial.print ("WiFi is ");
        Serial.print (WiFi.isConnected () ? "connected" : "not connected"); Serial.print (". ");
        Serial.print ("Uptime: ");
        Serial.print (NTP.getUptimeString ()); Serial.print (" since ");
        Serial.println (NTP.getTimeDateString (NTP.getFirstSync ()).c_str ());

        i++;
    }
    delay (0);
}
