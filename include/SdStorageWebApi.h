#pragma once

#include <ESPAsyncWebServer.h>
#include <TaskSchedulerDeclarations.h>

class SdStorageWebApiClass {
public:
    void init(AsyncWebServer& server, Scheduler& scheduler);
    void loop();
};