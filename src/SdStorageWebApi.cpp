// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Thomas Basler and others
 */
#include "SdStorageWebApi.h"
#include "FS.h"
#include <SPI.h>
#include <SD.h>
#include "SdStorage.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "Datastore.h"


void SdStorageWebApiClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

  server.on("/db/ui", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SD, "/database.html", "text/html");
  });

  server.on("/db/getdbdata", HTTP_GET, [](AsyncWebServerRequest *request){
    int timestamp = 0;
    //Check if GET parameter exists
    if(request->hasParam("timestamp")){
      const AsyncWebParameter* p = request->getParam("timestamp");
      timestamp = p->value().toInt();
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");

    serializeJson(SdStorage.getJsonFromLastData(timestamp), *response);

    request->send(response);
  });

  server.on("/db/getdaydata", HTTP_GET, [](AsyncWebServerRequest *request){
    int timestamp = 0;
    //Check if GET parameter exists
    if(request->hasParam("timestamp")){
      const AsyncWebParameter* p = request->getParam("timestamp");
      timestamp = p->value().toInt();
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");

    serializeJson(SdStorage.getJsonFromLogDayBuffer(timestamp), *response);

    request->send(response);
  });

  server.on("/db/getmonthdata", HTTP_GET, [](AsyncWebServerRequest *request){
    int timestamp = 0;
    //Check if GET parameter exists
    if(request->hasParam("timestamp")){
      const AsyncWebParameter* p = request->getParam("timestamp");
      timestamp = p->value().toInt();
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");

    serializeJson(SdStorage.getJsonFromLogMonthBuffer(timestamp), *response);

    request->send(response);
  });

  server.on("/db/getdbdaydata", HTTP_GET, [](AsyncWebServerRequest *request){
    int timestamp = 0;
    //Check if GET parameter exists
    if(request->hasParam("timestamp")){
      const AsyncWebParameter* p = request->getParam("timestamp");
      timestamp = p->value().toInt();
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");

    serializeJson(SdStorage.dbReadEntries(timestamp), *response);

    request->send(response);
  });

  server.on("/db/getyield", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("application/json");

    StaticJsonDocument<256> doc;
    JsonObject total_Power = doc.createNestedObject("Power");
    total_Power["v"] = Datastore.getTotalAcPowerEnabled();
    total_Power["u"] = "W";
    total_Power["d"] = 0;

    JsonObject total_YieldDay = doc.createNestedObject("YieldDay");
    total_YieldDay["v"] = Datastore.getTotalAcYieldDayEnabled();
    total_YieldDay["u"] = "Wh";
    total_YieldDay["d"] = 0;

    JsonObject total_YieldTotal = doc.createNestedObject("YieldTotal");
    total_YieldTotal["v"] = Datastore.getTotalAcYieldTotalEnabled();
    total_YieldTotal["u"] = "kWh";
    total_YieldTotal["d"] = 3;

    serializeJson(doc, *response);

    request->send(response);
  });
}

void SdStorageWebApiClass::loop()
{
}