#ifndef COLLECTINGDATA_H
#define COLLECTINGDATA_H

#include <string>
#include <fstream>
#include <ApplicationServices/ApplicationServices.h>

using namespace std;

extern ofstream outFile;
extern const string OUTPUT_FILE;

extern ofstream csvFile;
extern const string CSV_FILE;

int getUsbDeviceCount();
void setConnectionTime();
void measureAndRecordMetrics(int charCode, int eventType);
CGEventRef eventCallbackBADUSB(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
int startDataCollectionBadUSB();

#endif