#include "BadUSBDetector.h"
#include <iostream>

BadUSBDetector::BadUSBDetector(const char* model_path) {
    XGBoosterCreate(nullptr, 0, &booster);
    if (XGBoosterLoadModel(booster, model_path) != 0) {
        std::cerr << "[FATAL ERROR] Failed to load XGBoost model from " << model_path << "\n";
        exit(1);
    }
    std::cout << "[SYSTEM] XGBoost Neural Engine Loaded Successfully.\n";
}

BadUSBDetector::~BadUSBDetector() {
    XGBoosterFree(booster);
}

float BadUSBDetector::predict(float avg_flight, float avg_inter, float avg_hold) {
    float input_data[3] = {avg_flight, avg_inter, avg_hold};

    DMatrixHandle dmatrix;
    XGDMatrixCreateFromMat(input_data, 1, 3, -1.0, &dmatrix);

    bst_ulong out_len;
    const float *out_result;
    XGBoosterPredict(booster, dmatrix, 0, 0, 0, &out_len, &out_result);

    float probability = out_result[0];

    XGDMatrixFree(dmatrix);
    
    return probability;
}