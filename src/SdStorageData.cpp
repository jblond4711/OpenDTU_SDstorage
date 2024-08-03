#include "SdStorageData.h"

SdStorageData::SdStorageData()
{
    reset();
}

SdStorageData::SdStorageData(int writeFreq, char* sqlReadInput, char* sqlInsertInput, char* sqlUpdateInput)
{
  timerWrite.set(writeFreq);
  reset();
  sqlRead = sqlReadInput;
  sqlInsert = sqlInsertInput;
  sqlUpdate = sqlUpdateInput;
}

void SdStorageData::reset(){
    dataBufferLength = 0;
    dataBufferFilled = false;
    dataBufferLastElement = &dataBuffer[0];
    timerWrite.reset();
}

void SdStorageData::setDataBufferFilled(bool state){
    dataBufferFilled = state;
}

bool SdStorageData::getDataBufferFilled(){
    return dataBufferFilled;
}

int SdStorageData::getYieldByTimestamp(int timestamp){
  if(!dataBufferFilled)
    return 0;

  for (int i = 50; i--;){
    if(dataBuffer[i][0] == timestamp){
      return dataBuffer[i][1];
    }
  }

  return 0;
}

int SdStorageData::getDataPositionByTimestamp(int timestamp){
  if(!dataBufferFilled)
    return -1;

  for (int i = 50; i--;){
    if(dataBuffer[i][0] == timestamp){
      return i;
    }
  }

  return -1;

}

bool SdStorageData::addToBuffer(int timestamp, int yieldData){
    if(dataBufferLength < 50){
        dataBuffer[dataBufferLength][0] = timestamp;
        dataBuffer[dataBufferLength][1] = yieldData;
        dataBufferLastElement = &dataBuffer[dataBufferLength];
        dataBufferLength++;
        return true;
    }
    return false;
}

void SdStorageData::appendOrUpdateBuffer(int timestamp, int yieldData){
  if(!dataBufferFilled)
    return;

  int pos = getDataPositionByTimestamp(timestamp);

  if(pos > -1){
    if(yieldData > dataBuffer[pos][1])
        dataBuffer[pos][1] = yieldData;
    return;
  }

  if(dataBufferLength < 50){
    dataBuffer[dataBufferLength][0] = timestamp;
    dataBuffer[dataBufferLength][1] = yieldData;
    dataBufferLastElement = &dataBuffer[dataBufferLength];
    dataBufferLength++;
    return;
  }

  for(int i = 0; i < 49; i++){
    dataBuffer[i][0] = dataBuffer[i+1][0];
    dataBuffer[i][1] = dataBuffer[i+1][1];
  }
  dataBuffer[49][0] = timestamp;
  dataBuffer[49][1] = yieldData;

}

int* SdStorageData::getLastElement(){
    return (*dataBufferLastElement);
}

DynamicJsonDocument SdStorageData::getJsonFromLastData(int timestamp){
  DynamicJsonDocument doc(3072);
  for (int i = 50; i--;){
    if(dataBuffer[i][0] > timestamp){
      JsonArray arr = doc.createNestedArray();
      arr.add(dataBuffer[i][0]);
      arr.add(dataBuffer[i][1]);
    }
  }
  return doc;
}

char* SdStorageData::getSqlRead(){
  return sqlRead;
}

char* SdStorageData::getSqlInsert(){
  return sqlInsert;
}

char* SdStorageData::getSqlUpdate(){
  return sqlUpdate;
}

unsigned long& SdStorageData::getLogTimestampRef(){
  return logTimestamp;
}

bool SdStorageData::checkLogTime(unsigned long currentTimestamp){
  return (currentTimestamp > logTimestamp && logTimestamp > 0);
}

void SdStorageData::setLogStartYield(float yield){
  logStartYield = yield;
}

float SdStorageData::getLogStartYield(){
  return logStartYield;
}

int SdStorageData::getYieldPeriod(float currentYield){
  return (int) (((currentYield - logStartYield) * 1000) + offsetYield + 0.5);
}

void SdStorageData::setOffsetYield(int yield){
  offsetYield = yield;
}

int SdStorageData::getOffsetYield(){
  return offsetYield;
}

void SdStorageData::calcOffsetYield(){
  offsetYield = getYieldByTimestamp(logTimestamp);
}

void SdStorageData::setLastPeriodYield(int yield){
  lastPeriodYield = yield;
}

int SdStorageData::getLastPeriodYield(){
  return lastPeriodYield;
}