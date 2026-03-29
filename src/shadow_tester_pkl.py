import pandas as pd
import joblib
import os

# 1. Dynamically find the root 'design' folder (go up one level from 'src')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")

# 2. Safely build the path to the telemetry file
telemetry_path = os.path.join(ROOT_DIR, "post_catch_telemetry.csv")

if not os.path.exists(telemetry_path):
    print(f"[ERROR] {telemetry_path} not found. Run the C++ attack first!")
    exit()

df = pd.read_csv(telemetry_path)
X_live = df[['flight', 'inter', 'hold']]

# 3. Safely build the path to the scaler
scaler_path = os.path.join(ROOT_DIR, "shadow_models", "shadow_scaler.pkl")
scaler = joblib.load(scaler_path)
X_live_scaled = scaler.transform(X_live)

# 4. Define the list of models we want to check
model_files = {
    "Logistic Regression": os.path.join(ROOT_DIR, "shadow_models", "shadow_logistic_reg.pkl"),
    "KNN": os.path.join(ROOT_DIR, "shadow_models", "shadow_knn_k5.pkl"),
    "SVM": os.path.join(ROOT_DIR, "shadow_models", "shadow_svm_rbf.pkl"),
    "Random Forest": os.path.join(ROOT_DIR, "shadow_models", "shadow_random_forest.pkl"),
    "Neural Network": os.path.join(ROOT_DIR, "shadow_models", "shadow_neural_network.pkl")
}

print(f"--- SHADOW MODE EVALUATION ({len(X_live)} Keys) ---")

for name, file_path in model_files.items():
    if os.path.exists(file_path):
        model = joblib.load(file_path)

        # Predict: 1 = BadUSB, 0 = Human
        predictions = model.predict(X_live_scaled)

        detections = sum(predictions == 1)
        accuracy = (detections / len(predictions)) * 100

        print(f"[{name:18}] Identified {detections}/{len(predictions)} keys as Malicious ({accuracy:.2f}%)")
    else:
        print(f"[SKIP] {file_path} not found.")

# 5. Compare with the active XGBoost score already in the CSV
if 'xgboost_prob' in df.columns:
    xgb_detections = sum(df['xgboost_prob'] > 0.75)
    xgb_accuracy = (xgb_detections / len(df)) * 100
    print(f"[{'XGBoost (Active)':18}] Identified {xgb_detections}/{len(df)} keys as Malicious ({xgb_accuracy:.2f}%)")