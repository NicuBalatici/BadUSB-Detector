#ifndef DESIGN_COLLECTINGDATAHUMAN_H
#define DESIGN_COLLECTINGDATAHUMAN_H
#include <string>
#include <ApplicationServices/ApplicationServices.h>
#include <fstream>

using namespace std;
using namespace std::chrono;

extern const string CSV_FILE_HUMAN_DATA;
extern ofstream csvFile_human;

using Clock = steady_clock;
using TimePoint = time_point<Clock>;

std::string getSafeChar(int charCode);
CGEventRef eventCallbackHUMAN(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
void startDataCollectionHuman();

#endif //DESIGN_COLLECTINGDATAHUMAN_H