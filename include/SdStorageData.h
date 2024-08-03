#pragma once

#include "ArduinoJson.h"
#include <TimeoutHelper.h>

class SdStorageData {
public:
    SdStorageData();
    SdStorageData(int writeFreq, char* sqlRead, char* sqlInsert, char* sqlUpdate);

    void reset();
    bool addToBuffer(int timestamp, int yieldData);
    bool fillBuffer();
    void appendToBuffer(int timestamp, int yieldData);
    void appendOrUpdateBuffer(int timestamp, int yieldData);
    int getYieldByTimestamp(int timestamp);//returns yield in lastDataArr by timestamp
    int getDataPositionByTimestamp(int timestamp); //returns position in lastDataArr by timestamp
    int* getLastElement();
    void setDataBufferFilled(bool state);
    bool getDataBufferFilled();
    char* getSqlRead();
    char* getSqlInsert();
    char* getSqlUpdate();
    DynamicJsonDocument getJsonFromLastData(int timestamp);
    void setLogTimestamp(unsigned long timestamp);
    unsigned long& getLogTimestampRef();
    bool checkLogTime(unsigned long currentTimestamp);
    void setLogStartYield(float yield);
    float getLogStartYield();
    int getYieldPeriod(float currentYield);
    void setOffsetYield(int yield);
    int getOffsetYield();
    void calcOffsetYield();
    void setLastPeriodYield(int yield);
    int getLastPeriodYield();
    TimeoutHelper timerWrite;

private:
    int dataBuffer[50][2] = {0};
    int dataBufferLength;
    int (*dataBufferLastElement)[2];
    bool dataBufferFilled;
    char* sqlRead;
    char* sqlInsert;
    char* sqlUpdate;
    unsigned long logTimestamp = 0;
    float logStartYield = 0;
    int offsetYield = 0;
    int lastPeriodYield = 0;

};