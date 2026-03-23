#ifndef BAD_USB_DETECTOR_H
#define BAD_USB_DETECTOR_H

#include <xgboost/c_api.h>

class BadUSBDetector {
private:
    BoosterHandle booster{};

public:
    BadUSBDetector(const char* model_path);
    ~BadUSBDetector();
    float predict(float avg_flight, float avg_inter, float avg_hold);
};

#endif