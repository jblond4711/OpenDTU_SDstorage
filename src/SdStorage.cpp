#include "SdStorage.h"
#include <time.h>
#include "Datastore.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"


SdStorageClass::SdStorageClass()
{
}

void SdStorageClass::init(Scheduler& scheduler)
{
  Serial.println("initializing SdStorage...");
  hspi = new SPIClass(/*HSPI*/);
  hspi->begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS);
  initSDCard();

  //init SQLite Database
  sqlite3_initialize();

  Serial.println("...SdStorage initiated");

  updateTimer.set(10000);
  writingDB = false;

  fillDataBuffer(hourYieldDataBuffer);
  fillDataBuffer(dayYieldDataBuffer);

  scheduler.addTask(_loopTask);
  _loopTask.setCallback(std::bind(&SdStorageClass::loop, this));
  _loopTask.setIterations(TASK_FOREVER);
  _loopTask.enable();

  return;
}

void SdStorageClass::loop()
{
  if (updateTimer.occured()) {
    updateTimer.reset();

    unsigned long currentTimestamp = get_Epoch_Time();
    float currentAcYieldTotal = Datastore.getTotalAcYieldTotalEnabled();

    if(!hourYieldDataBuffer.getDataBufferFilled())
      fillDataBuffer(hourYieldDataBuffer);
    if(!dayYieldDataBuffer.getDataBufferFilled())
      fillDataBuffer(dayYieldDataBuffer);
    if(!monthYieldDataBuffer.getDataBufferFilled())
      fillDataBuffer(monthYieldDataBuffer);

    if(!currentTimestamp || !(currentAcYieldTotal > 0))
      return;

    unsigned long& logTimestampHour = hourYieldDataBuffer.getLogTimestampRef();
    unsigned long& logTimestampDay = dayYieldDataBuffer.getLogTimestampRef();
    unsigned long& logTimestampMonth = monthYieldDataBuffer.getLogTimestampRef();

    if(!logTimestampHour){
      logTimestampHour = logHour(currentTimestamp);
      hourYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
      hourYieldDataBuffer.calcOffsetYield();

      logTimestampDay = logDay(currentTimestamp);
      dayYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
      dayYieldDataBuffer.calcOffsetYield();

      logTimestampMonth = logMonth();
      monthYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
      monthYieldDataBuffer.calcOffsetYield();

      hourYieldDataBuffer.timerWrite.reset();
      dayYieldDataBuffer.timerWrite.reset();
      monthYieldDataBuffer.timerWrite.reset();
      return;
    }

    const unsigned long nowLogHour = logHour(currentTimestamp);
    const int totalYieldHour = hourYieldDataBuffer.getYieldPeriod(currentAcYieldTotal);
    const int totalYieldDay = dayYieldDataBuffer.getYieldPeriod(currentAcYieldTotal);
    const int totalYieldMonth = monthYieldDataBuffer.getYieldPeriod(currentAcYieldTotal);
    

    if(hourYieldDataBuffer.checkLogTime(nowLogHour)){
      if(totalYieldHour > 0 || hourYieldDataBuffer.getLastPeriodYield() > 0){
        hourYieldDataBuffer.appendOrUpdateBuffer(logTimestampHour, totalYieldHour);
        writeDataToDB(logTimestampHour, totalYieldHour, hourYieldDataBuffer);
      }
      logTimestampHour = nowLogHour;
      hourYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
      hourYieldDataBuffer.setLastPeriodYield(totalYieldHour);
      hourYieldDataBuffer.setOffsetYield(0);
    }
    else if (hourYieldDataBuffer.timerWrite.occured()){
      hourYieldDataBuffer.timerWrite.reset();

      if(totalYieldHour > 0 || hourYieldDataBuffer.getLastPeriodYield() > 0){
        const unsigned long timestampLastHour = logTimestampHour - 3600;
        if(hourYieldDataBuffer.getLastElement()[0] < timestampLastHour){
          Serial.printf("add Zero-Data before new Data");
          hourYieldDataBuffer.appendOrUpdateBuffer(timestampLastHour, 0);
          writeDataToDB(timestampLastHour, 0, hourYieldDataBuffer);
        }

        hourYieldDataBuffer.appendOrUpdateBuffer(logTimestampHour, totalYieldHour);
        writeDataToDB(logTimestampHour, totalYieldHour, hourYieldDataBuffer);
      }
    }

    dayYieldDataBuffer.appendOrUpdateBuffer(logTimestampDay, totalYieldDay);

    if(dayYieldDataBuffer.checkLogTime(logDay(currentTimestamp))){
        writeDataToDB(logTimestampDay, totalYieldDay, dayYieldDataBuffer);
        logTimestampDay = logDay(currentTimestamp);
        dayYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
        dayYieldDataBuffer.setOffsetYield(0);
        dayYieldDataBuffer.timerWrite.reset();
    }
    else if(dayYieldDataBuffer.timerWrite.occured()){
      writeDataToDB(logTimestampDay, totalYieldDay, dayYieldDataBuffer);
      dayYieldDataBuffer.timerWrite.reset();
    }

    monthYieldDataBuffer.appendOrUpdateBuffer(logTimestampMonth, totalYieldMonth);

    if(monthYieldDataBuffer.checkLogTime(logMonth())){
        writeDataToDB(logTimestampMonth, totalYieldMonth, monthYieldDataBuffer);
        logTimestampMonth = logMonth();
        monthYieldDataBuffer.setLogStartYield(currentAcYieldTotal);
        monthYieldDataBuffer.setOffsetYield(0);
        monthYieldDataBuffer.timerWrite.reset();
    }
    else if(monthYieldDataBuffer.timerWrite.occured()){
      writeDataToDB(logTimestampMonth, totalYieldMonth, monthYieldDataBuffer);
      monthYieldDataBuffer.timerWrite.reset();
    }

    

  return;
  }
}

void SdStorageClass::initSDCard(){
  if(!SD.begin(SD_SS, *hspi)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
}

int SdStorageClass::openDb(const char *filename, sqlite3 **db) {
   int rc = sqlite3_open(filename, db);
   if (rc) {
       Serial.printf("Can't open database: %s\n", sqlite3_errmsg(*db));
       return rc;
   } else {
       Serial.printf("Opened database successfully\n");
   }
   return rc;
}

DynamicJsonDocument SdStorageClass::dbReadEntries(int timestamp){
  Serial.println("Free Heap before DBread: ");
  Serial.println(ESP.getFreeHeap());


  DynamicJsonDocument doc(512);
  if(writingDB){
    Serial.println("DB occupied by writing...");
    return doc;
  }

  if(!timestamp)
    timestamp = get_Epoch_Time();

  int rc;
  sqlite3_stmt *res;
  const char *tail;
  int rec_count = 0;

  char sql[128];
  char* txt = "Select * from solarDay WHERE timestamp < %u  ORDER BY timestamp DESC LIMIT 10;";
  sprintf(sql, txt, timestamp);

  if (openDb("/sd/solarData.db", &db1)){
    Serial.println("Failed open DB");
    return doc;
  }

  rc = sqlite3_prepare_v2(db1, sql, strlen(sql), &res, &tail);
  if (rc != SQLITE_OK) {
    Serial.println("Failed to fetch data: ");
    Serial.println(sqlite3_errmsg(db1));
    return doc;
  }

  while (sqlite3_step(res) == SQLITE_ROW) {
    JsonArray arr = doc.createNestedArray();
    rec_count++;
    arr.add(sqlite3_column_int(res, 0));
    arr.add(sqlite3_column_int(res, 1));
  }
  Serial.println("Free Heap after DBread: ");
  Serial.println(ESP.getFreeHeap());

  Serial.println("Number of found DB entries: ");
  Serial.println(rec_count);


  sqlite3_finalize(res);

  sqlite3_close(db1);

  return doc;
}

bool SdStorageClass::fillDataBuffer(SdStorageData& dataObj){

  if(writingDB){
    Serial.println("DB occupied by writing...");
    return false;
  }

  if (openDb("/sd/solarData.db", &db1)){
    Serial.println("Failed open DB");
    return false;
  }

  int rc;
  sqlite3_stmt *res;
  const char *tail;
  int rec_count = 0;
  const char *sql = dataObj.getSqlRead();

  rc = sqlite3_prepare_v2(db1, sql, strlen(sql), &res, &tail);
  if (rc != SQLITE_OK) {
    Serial.println("Failed to fetch data: ");
    Serial.println(sqlite3_errmsg(db1));
    return false;
  }


  while (sqlite3_step(res) == SQLITE_ROW && rec_count < 50) {
    dataObj.addToBuffer(sqlite3_column_int(res, 0), sqlite3_column_int(res, 1));
    rec_count++;
  }

  dataObj.setDataBufferFilled(true);
  Serial.println("lastDataArr filled with: ");
  Serial.println(rec_count);
  Serial.println("Free Heap after DBread: ");
  Serial.println(ESP.getFreeHeap());
 // Serial.println("ArduinoJson Array: ");
 // serializeJson(doc, Serial);

  Serial.println("Number of found DB entries: ");
  Serial.println(rec_count);

  sqlite3_finalize(res);
  sqlite3_close(db1);

  return true;
}

DynamicJsonDocument SdStorageClass::getJsonFromLastData(int timestamp){
  return hourYieldDataBuffer.getJsonFromLastData(timestamp);
}

// Get_Epoch_Time() Function that gets current epoch time
unsigned long SdStorageClass::get_Epoch_Time() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void SdStorageClass::printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

unsigned long SdStorageClass::logHour(time_t timestamp){
  return (timestamp - ((timestamp + 1800) % 3600) + 1800);
}

unsigned long SdStorageClass::logDay(time_t timestamp){
  return (timestamp - (timestamp % 86400) + 43200);
}

unsigned long SdStorageClass::logMonth(){
  time_t timestamp;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  timeinfo.tm_mday = 1;
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;

  timestamp = mktime(&timeinfo);

  return timestamp;
}

unsigned long SdStorageClass::logYear(){
  time_t timestamp;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return(0);
  }
  timeinfo.tm_mon = 0;
  timeinfo.tm_mday = 1;
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;

  timestamp = mktime(&timeinfo);

  return timestamp;
}

bool SdStorageClass::writeDataToDB(unsigned long timestamp, int yieldValue, SdStorageData& dataObj){
  writingDB = true;

  if (isnan(timestamp) || isnan(yieldValue)) {
    Serial.println("Failed to write data to DB. Data is nan");
    writingDB = false;
    return false;
  }

  if (openDb("/sd/solarData.db", &db1)){
    writingDB = false;
    return false;
  }

  int rc;
  sqlite3_stmt *res;
  const char *tail;
  const char *sql = dataObj.getSqlUpdate();
  
  rc = sqlite3_prepare_v2(db1, sql, strlen(sql), &res, &tail);
  if (rc != SQLITE_OK) {
     Serial.printf("ERROR preparing sql: %s\n", sqlite3_errmsg(db1));
     Serial.println("SQL Statement: ");
     Serial.println(sql);
     sqlite3_close(db1);
     writingDB = false;
     return false;
     }

  sqlite3_bind_int(res, 1, timestamp);
  //sqlite3_bind_double(res, 2, millis()*0.001);
  sqlite3_bind_int(res, 2, yieldValue);

  if (sqlite3_step(res) != SQLITE_DONE) {
      Serial.printf("ERROR executing stmt: %s\n", sqlite3_errmsg(db1));
      sqlite3_close(db1);
      writingDB = false;
      return false;
    }
    sqlite3_clear_bindings(res);
    rc = sqlite3_reset(res);

    if (rc != SQLITE_OK) {
      sqlite3_close(db1);
      writingDB = false;
    return true;
  }

  sqlite3_finalize(res);


  const char *sql2 = dataObj.getSqlInsert();

  rc = sqlite3_prepare_v2(db1, sql2, strlen(sql2), &res, &tail);
  if (rc != SQLITE_OK) {
     Serial.printf("ERROR preparing sql: %s\n", sqlite3_errmsg(db1));
     Serial.println("SQL Statement: ");
     Serial.println(sql2);
     sqlite3_close(db1);
     writingDB = false;
     return false;
     }

  sqlite3_bind_int(res, 1, timestamp);
  //sqlite3_bind_double(res, 2, millis()*0.001);
  sqlite3_bind_int(res, 2, yieldValue);

  if (sqlite3_step(res) != SQLITE_DONE) {
      Serial.printf("ERROR executing stmt: %s\n", sqlite3_errmsg(db1));
      sqlite3_close(db1);
      writingDB = false;
      return false;
    }

  sqlite3_clear_bindings(res);
  rc = sqlite3_reset(res);

  if (rc != SQLITE_OK) {
    sqlite3_close(db1);
    writingDB = false;
    return true;
  }

  sqlite3_finalize(res);

  sqlite3_close(db1);
  Serial.println("write to DB successfull");
  writingDB = false;
  return true;
}

DynamicJsonDocument SdStorageClass::getJsonFromLogDayBuffer(int timestamp){
  return dayYieldDataBuffer.getJsonFromLastData(timestamp);
}

DynamicJsonDocument SdStorageClass::getJsonFromLogMonthBuffer(int timestamp){
  return monthYieldDataBuffer.getJsonFromLastData(timestamp);
}

SdStorageClass SdStorage;