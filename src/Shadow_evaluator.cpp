#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include "ShadowModels.h"

using namespace std;

void runNativeModelEvaluation() {
    cout << "\n==================================================\n";
    cout << "   NATIVE C++ SHADOW MODEL EVALUATION ENGINE      \n";
    cout << "==================================================\n\n";

    // 1. Find the telemetry file
    string filename = "post_catch_telemetry.csv";
    ifstream file(filename);
    
    if (!file.is_open()) {
        filename = "shadow_telemetry.csv";
        file.open(filename);
        if (!file.is_open()) {
            cerr << "[ERROR] Could not find telemetry CSV file. Run an attack first!\n";
            return;
        }
    }

    cout << "[INFO] Successfully loaded dataset: " << filename << "\n";

    string line;
    bool is_header = true;
    
    // Counters for accuracy
    int total_keys = 0;
    int logreg_caught = 0;
    int svm_caught = 0;
    int rf_caught = 0;
    int nn_caught = 0;
    int xgb_active_caught = 0;

    // 2. Parse the CSV line by line
    while (getline(file, line)) {
        if (is_header) {
            is_header = false;
            continue; // Skip the "flight,inter,hold,xgboost_prob" header
        }

        stringstream ss(line);
        string cell;
        vector<double> row_data;

        // Split by comma
        while (getline(ss, cell, ',')) {
            try {
                row_data.push_back(stod(cell));
            } catch (...) {
                row_data.push_back(0.0);
            }
        }

        if (row_data.size() >= 3) {
            double flight = row_data[0];
            double inter = row_data[1];
            double hold = row_data[2];
            
            // Track Active XGBoost predictions from the CSV (if present)
            if (row_data.size() >= 4 && row_data[3] > 0.75) {
                xgb_active_caught++;
            }

            // 3. Scale the features natively!
            double scaled_features[3];
            scale_features(flight, inter, hold, scaled_features);

            // 4. Run the Native C++ Models
            // m2cgen standard models output a margin where > 0 is Malicious
            if (predict_logistic_reg(scaled_features) > 0) logreg_caught++;
            if (predict_svm(scaled_features) > 0) svm_caught++;

            double rf_output[2];
            predict_random_forest(scaled_features, rf_output);
            if (rf_output[1] > rf_output[0]) rf_caught++;

            // Our custom Neural Network outputs a probability where > 0.5 is Malicious
            if (predict_neural_network(scaled_features) > 0.5) nn_caught++;

            total_keys++;
        }
    }
    file.close();

    if (total_keys == 0) {
        cout << "[WARNING] The CSV file is empty!\n";
        return;
    }

    // 5. Calculate and print the Leaderboard
    cout << "\n--- NATIVE C++ INFERENCE RESULTS (" << total_keys << " Keys Tested) ---\n";
    
    auto print_result = [total_keys](const string& name, int caught) {
        double accuracy = ((double)caught / total_keys) * 100.0;
        cout << " -> " << left << setw(20) << name 
             << ": Caught " << setw(3) << caught << "/" << total_keys 
             << " (" << fixed << setprecision(2) << accuracy << "%)\n";
    };

    print_result("Random Forest", rf_caught);
    print_result("Support Vector Machine", svm_caught);
    print_result("Neural Network", nn_caught);
    print_result("Logistic Regression", logreg_caught);
    print_result("XGBoost (Active)", xgb_active_caught);
    
    cout << "==================================================\n\n";
}