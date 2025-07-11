/*
   kvv-oled.ino - Martin Schmidt

   based on the excellent work done in
   kvv.ino - Till Harbaum <till@harbaum.org>

   Display the departure information for KVV tram lines in Karlsruhe
   on a SSD1306 display. The code was adopted to work on an esp32
   and with the reduced character space available on the SD1306 Dis-
   plays.

   While this does not stay true to the original "power-saving"
   effort of the e-ink display, the constant power draw is used to
   cycle through the station names.
*/

#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <time.h>

#include "wifi.h"
#include "FreeSansBold9pt8b.h"

// stop IDs can be found in the JSON reply for e.g. Pionierstraße using this request in a regular browser:
// https://www.kvv.de/tunnelEfaDirect.php?action=XSLT_STOPFINDER_REQUEST&name_sf=pionierstraße&outputFormat=JSON&type_sf=any
//#define STOP_ID  "7000238"  // Pionierstraße
//#define STOP_ID  "7001004"    // Europaplatz/Postgalerie
//#define STOP_ID "7000065"  // ZKM
#define STOP_ID "7000063" // Kolpingplatz
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define LIMIT "3"  // the OLED display can display six text lines

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiMulti WiFiMulti;

struct Service {
  String route;
  String direction;
  String time;
} services[3];

unsigned int offsetCounter = 0;

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  // wait for WiFi link up
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print('.');
    digitalWrite(LED_BUILTIN, HIGH);  // led off
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);  // led on
    delay(50);
  }
}

void loop() {
  queryKVV();
  for (int d = 0; d < 120; d++) {
    display.clearDisplay();
    display.setFont(&FreeSansBold9pt8b);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    for (int i = 0; i < 3; i++) {
      display.setCursor(0, i * 20 + 20);
      display.print(services[i].route);
      display.print(" ");
      display.print(utf8ascii(rotateString(services[i].direction + " ", 6)));
      display.setCursor(80, i * 20 + 20);
      display.print(services[i].time);
    }
    display.display();
    offsetCounter++;
    delay(500);
  }
}

void queryKVV() {
  NetworkClientSecure *client = new NetworkClientSecure;
  if (client) {
    client->setInsecure();
    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?"
                             "outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&"
                             "locationServerActive=1&mode=direct&name_dm=" STOP_ID "&type_dm=stop&"
                             "useOnlyStops=1&useRealtime=1&limit=" LIMIT)) {

      //if (https.begin(*client, "https://jigsaw.w3.org/HTTP/connection.html")) {
      Serial.print("[HTTPS] GET...");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf(" code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          //Serial.println(payload);
          parse_reply(payload);
        }
      } else
        Serial.printf(" failed, error: %s\n", https.errorToString(httpCode).c_str());
      https.end();
    } else
      Serial.printf("[HTTPS] Unable to connect\n");
  } else {
    Serial.printf("Unable to create client");
  }
}

// ****** UTF8-Decoder: convert UTF8-string to extended ASCII *******

// Convert a single Character from UTF8 to Extended ASCII
// Return "0" if a byte has to be ignored
byte utf8ascii(byte ascii) {
  static byte c1;  // Last character buffer

  if (ascii < 128) {  // Standard ASCII-set 0..0x7F handling
    c1 = 0;
    return (ascii);
  }

  // get previous input
  byte last = c1;  // get last char
  c1 = ascii;      // remember actual character

  switch (last) {  // conversion depending on first UTF8-character
    case 0xC2: return (ascii); break;
    case 0xC3: return (ascii | 0xC0); break;
    case 0x82:
      if (ascii == 0xAC) return (0x80);  // special case Euro-symbol
  }

  return (0);  // otherwise: return zero, if character has to be ignored
}

// convert String object from UTF8 String to Extended ASCII
String utf8ascii(String s) {
  String r = "";
  char c;
  for (int i = 0; i < s.length(); i++) {
    c = utf8ascii(s.charAt(i));
    if (c != 0) r += c;
  }
  return r;
}

String rotateString(String s, int length) {
  int offset = offsetCounter % s.length();
  if (offset + length <= s.length()) {
    return s.substring(offset, offset + length);
  }
  return s.substring(offset, s.length()) + s.substring(0, length - (s.length() - offset));
}

void parse_time(struct tm *timeinfo, const JsonObject &obj) {
  memset(timeinfo, 0, sizeof(struct tm));
  if (obj.containsKey("year")) timeinfo->tm_year = atoi(obj["year"]) - 1900;
  if (obj.containsKey("month")) timeinfo->tm_mon = atoi(obj["month"]) - 1;
  if (obj.containsKey("day")) timeinfo->tm_mday = atoi(obj["day"]);
  if (obj.containsKey("hour")) timeinfo->tm_hour = atoi(obj["hour"]);
  if (obj.containsKey("minute")) timeinfo->tm_min = atoi(obj["minute"]);
}

void parse_reply(String payload) {
  int16_t x, y;
  uint16_t w, h;

  DynamicJsonDocument doc(4096);

  // install filters to extract only the required information from the stream
  StaticJsonDocument<300> filter;
  filter["dateTime"] = true;
  filter["dm"]["points"]["point"]["name"] = true;  // station name
  filter["departureList"][0]["countdown"] = true;
  filter["departureList"][0]["realDateTime"]["hour"] = true;
  filter["departureList"][0]["realDateTime"]["minute"] = true;
  filter["departureList"][0]["dateTime"]["hour"] = true;
  filter["departureList"][0]["dateTime"]["minute"] = true;
  filter["departureList"][0]["servingLine"]["direction"] = true;
  filter["departureList"][0]["servingLine"]["symbol"] = true;

  // start parsing
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();

  // get stop name
  const char *stopName = obj["dm"]["points"]["point"]["name"];
  // find last comma in string and cut at the first non-space afterwards
  // to get rid of city name
  const char *c = stopName;
  for (int i = 0; i < strlen(c); i++)
    if (c[i] == ',') {
      for (i++; c[i] == ' '; i++);
      stopName = c + i;
    }

  // parse time from reply into timeinfo
  struct tm timeinfo;
  parse_time(&timeinfo, obj["dateTime"]);

  // one would usually use strftime, but that adds leading 0's to the
  // month and hour which we don't want to save space
  // max length of timestring is "DD.MM.YY HH:mm" -> 15 Bytes incl \0-term
  char timeStamp[15];
  sprintf(timeStamp, "%d.%d.%02d %d:%02d",
          timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year - 100,
          timeinfo.tm_hour, timeinfo.tm_min);

  Serial.printf("Name: %s\n", stopName);
  Serial.printf("Timestamp: %s\n", timeStamp);

  // Delayed trains are listed first even if their real departure time
  // is later then other trains. We'd like to display them in their real
  // departure order instead. For simplicity, we just do a very rudimentary sort.

  // Create a list for the indices ordered by countdown value. This is done by
  // list as JsonObjs cannot trivially be copied.
  int order[obj["departureList"].size()];
  for (size_t i = 0; i < obj["departureList"].size(); i++) order[i] = i;

  for (size_t i = 0; i < obj["departureList"].size() - 1; i++) {
    for (size_t j = i + 1; j < obj["departureList"].size(); j++) {
      int ci = obj["departureList"][order[i]]["countdown"];
      int cj = obj["departureList"][order[j]]["countdown"];
      if (ci > cj) {  // Swap elements
        int temp = order[i];
        order[i] = order[j];
        order[j] = temp;
      }
    }
  }

  for (int i = 0; i < obj["departureList"].size(); i++) {
    JsonObject nobj = obj["departureList"][i];  // i'th object in departure list

    // some directions include redirects of the form "> next line", like e.g.
    // "Rheinbergstraße > 75 Bruchweg"
    // This is too long for the small display, so we get rid of everything
    // after the ">"
    const char *direction = nobj["servingLine"]["direction"];
    char *c = (char *)direction;
    while (*c && *c != '>') c++;  // search for '>'
    if (*c) {                     // '>' was found
      c--;                        // skip before '>'
      while (*c == ' ') c--;      // skip back over whitespaces
      c[1] = '\0';                // terminate string after last non-space
    }

    // further direction/destination handlng requires a String object
    String destination(direction);

    const char *route = nobj["servingLine"]["symbol"];

    // countdown are the minutes to go
    int countdown = atoi(nobj["countdown"]);

    // get readDateTime if present, otherwise dateTime
    struct tm deptime;
    if (nobj.containsKey("realDateTime")) parse_time(&deptime, nobj["realDateTime"]);
    else parse_time(&deptime, nobj["dateTime"]);

    // Create nice time string. Countdown is actually sometimes < 0 if the vehicle
    // was expected to arrive already
    char time[8];
    if (countdown <= 0) strcpy(time, "0 min");
    else if (countdown < 10) sprintf(time, "%d min", countdown);
    else sprintf(time, "%d:%02d", deptime.tm_hour, deptime.tm_min);

    services[i].route = route;
    services[i].direction = direction;
    services[i].time = time;
    Serial.printf("[%s] %s %s\n", route, direction, time);
  }
}
