#pragma once

#include "FS.h"
#include <SPI.h>
#include <SD.h>
#include <sqlite3.h>
#include <ESPAsyncWebServer.h>
#include <TimeoutHelper.h>
#include <TaskSchedulerDeclarations.h>
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include "SdStorageData.h"


#define SD_SCK  GPIO_NUM_14
#define SD_MISO GPIO_NUM_33
#define SD_MOSI GPIO_NUM_13
#define SD_SS   GPIO_NUM_15

class SdStorageClass {
public:
    SdStorageClass();
    //~SdStorageClass();

    void init(Scheduler& scheduler);
    void loop();
    DynamicJsonDocument dbReadEntries(int timestamp);
    DynamicJsonDocument getJsonFromLastData(int timestamp);
    DynamicJsonDocument getJsonFromLogDayBuffer(int timestamp);
    DynamicJsonDocument getJsonFromLogMonthBuffer(int timestamp);
    DynamicJsonDocument getJsonFromLogYearBuffer(int timestamp);

private:
    Task _loopTask;
    SPIClass* hspi = NULL;
    sqlite3* db1;
    bool writingDB;
    TimeoutHelper updateTimer;

    SdStorageData hourYieldDataBuffer{60000, "Select * from (Select * from solar ORDER BY timestamp DESC LIMIT 50) ORDER BY timestamp ASC;", "INSERT OR IGNORE INTO solar (timestamp, yieldHour) VALUES (?1, ?2)", "UPDATE solar SET yieldHour = ?2 WHERE (timestamp = ?1 AND yieldHour < ?2)"};
    SdStorageData dayYieldDataBuffer{900000, "Select * from (Select * from solarDay ORDER BY timestamp DESC LIMIT 50) ORDER BY timestamp ASC;", "INSERT OR IGNORE INTO solarDay (timestamp, yieldDay) VALUES (?1, ?2)", "UPDATE solarDay SET yieldDay = ?2 WHERE (timestamp = ?1 AND yieldDay < ?2)"};
    SdStorageData monthYieldDataBuffer{3600000, "Select * from (Select * from solarMonth ORDER BY timestamp DESC LIMIT 50) ORDER BY timestamp ASC;", "INSERT OR IGNORE INTO solarMonth (timestamp, yield) VALUES (?1, ?2)", "UPDATE solarMonth SET yield = ?2 WHERE (timestamp = ?1 AND yield < ?2)"};


    void initSDCard();
    int openDb(const char *filename, sqlite3 **db);
    unsigned long get_Epoch_Time();
    void printLocalTime();
    unsigned long logHour(time_t timestamp); //calculate which is the current hour for data log
    unsigned long logDay(time_t timestamp); //calculate which is the current day for data log
    unsigned long logMonth(); //calculate which is the current month for data log
    unsigned long logYear(); //calculate which is the current year for data log
    bool writeDataToDB(unsigned long timestamp, int totalAcYieldHour, SdStorageData& dataObj);
    bool fillDataBuffer(SdStorageData& dataObj);
};

extern SdStorageClass SdStorage;
