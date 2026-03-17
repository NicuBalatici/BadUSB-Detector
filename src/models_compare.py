import pandas as pd
import numpy as np
import os
import matplotlib.pyplot as plt
import seaborn as sns

from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, f1_score, roc_auc_score, roc_curve, confusion_matrix

# Import the ML Algorithms
from sklearn.linear_model import LogisticRegression
from sklearn.neighbors import KNeighborsClassifier
from sklearn.svm import SVC
from sklearn.ensemble import RandomForestClassifier
from xgboost import XGBClassifier
from sklearn.neural_network import MLPClassifier

# 1. SETUP DIRECTORY FOR ML GRAPHS
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
output_dir = os.path.join(SCRIPT_DIR, "../images/ml_results")
if not os.path.exists(output_dir):
    os.makedirs(output_dir)
    print(f"Created directory: {output_dir}")

# 2. LOAD & CLEAN DATA
badusb_path = os.path.join(SCRIPT_DIR, "../data/training_data/badusb_data.csv")
human_path = os.path.join(SCRIPT_DIR, "../data/training_data/human_data.csv")

badusb = pd.read_csv(badusb_path, on_bad_lines='skip')
human = pd.read_csv(human_path, on_bad_lines='skip')

# Fix data leakage
badusb = badusb.rename(columns=lambda x: x.replace(';', '').strip())
human = human.rename(columns=lambda x: x.replace(';', '').strip())

if 'label' in badusb.columns: badusb = badusb.drop(columns=['label'])
if 'label' in human.columns: human = human.drop(columns=['label'])

badusb["label"] = 1
human["label"] = 0
data = pd.concat([badusb, human], ignore_index=True)

# Keep only numbers and drop NaNs
X = data.drop(columns=["label"]).select_dtypes(include=[np.number])
data_clean = pd.concat([X, data["label"]], axis=1).dropna()
X = data_clean.drop(columns=["label"])
y = data_clean["label"]

# 3. SPLIT AND SCALE THE DATA
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

# Scaling is CRITICAL for Neural Networks to converge properly!
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

# 4. DEFINE THE 6 MODELS
models = {
    "Logistic Reg": LogisticRegression(random_state=42),
    "KNN (k=5)": KNeighborsClassifier(n_neighbors=5),
    "SVM (RBF)": SVC(probability=True, random_state=42),
    "Random Forest": RandomForestClassifier(n_estimators=100, random_state=42),
    "XGBoost": XGBClassifier(use_label_encoder=False, eval_metric='logloss', random_state=42),
    # The New Neural Network! 2 hidden layers (64 neurons, 32 neurons)
    "Neural Network": MLPClassifier(hidden_layer_sizes=(64, 32), max_iter=500, random_state=42)
}

# 5. TRAIN AND EVALUATE
results = []
roc_data = {}
cm_data = {}

print("Training Models & Generating Predictions... (Neural Net may take a few extra seconds)")

for name, model in models.items():
    model.fit(X_train_scaled, y_train)

    y_pred = model.predict(X_test_scaled)
    y_prob = model.predict_proba(X_test_scaled)[:, 1]

    # Calculate Metrics
    acc = accuracy_score(y_test, y_pred)
    f1 = f1_score(y_test, y_pred)
    auc = roc_auc_score(y_test, y_prob)

    # Calculate ROC Curve points
    fpr, tpr, _ = roc_curve(y_test, y_prob)

    # Store data for plotting
    results.append({"Model": name, "Accuracy": acc, "F1-Score": f1, "ROC-AUC": auc})
    roc_data[name] = {"fpr": fpr, "tpr": tpr, "auc": auc}
    cm_data[name] = confusion_matrix(y_test, y_pred)

    print(f"[{name}] Trained. AUC: {auc:.4f}")

results_df = pd.DataFrame(results)

# =========================================================
# GRAPH 1: Performance Metrics Bar Chart
# =========================================================
print("\nGenerating Metrics Comparison Graph...")
results_melted = results_df.melt(id_vars="Model", var_name="Metric", value_name="Score")

plt.figure(figsize=(14, 6))  # Made slightly wider to fit 6 names comfortably
sns.barplot(data=results_melted, x="Model", y="Score", hue="Metric", palette="viridis")
plt.title("Machine Learning vs Deep Learning: BadUSB Detection", fontsize=16)
plt.ylim(0.5, 1.05)
plt.ylabel("Score (0 to 1)")
plt.legend(loc='lower right')
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "1_metrics_comparison.png"))
plt.close()

# =========================================================
# GRAPH 2: Combined ROC Curves
# =========================================================
print("Generating Combined ROC Curves...")
plt.figure(figsize=(10, 8))

# Added a 6th color (magenta) for the Neural Net
colors = ['blue', 'green', 'orange', 'purple', 'red', 'magenta']
for (name, data), color in zip(roc_data.items(), colors):
    plt.plot(data['fpr'], data['tpr'], label=f"{name} (AUC = {data['auc']:.3f})", color=color, linewidth=2)

plt.plot([0, 1], [0, 1], 'k--', label='Random Guessing (AUC = 0.500)')

plt.title('Receiver Operating Characteristic (ROC) Curves', fontsize=16)
plt.xlabel('False Positive Rate (Incorrectly flagging a Human)')
plt.ylabel('True Positive Rate (Catching the BadUSB)')
plt.legend(loc='lower right', fontsize=12)
plt.grid(alpha=0.3)
plt.tight_layout()
plt.savefig(os.path.join(output_dir, "2_roc_curves.png"))
plt.close()

# =========================================================
# GRAPH 3: Confusion Matrix Grid (Perfect 2x3 now)
# =========================================================
print("Generating Confusion Matrix Grid...")
fig, axes = plt.subplots(2, 3, figsize=(15, 10))
fig.suptitle('Confusion Matrices by Model', fontsize=20)
axes = axes.flatten()

for i, (name, cm) in enumerate(cm_data.items()):
    sns.heatmap(cm, annot=True, fmt='d', cmap='Blues', ax=axes[i],
                xticklabels=['Human', 'BadUSB'], yticklabels=['Human', 'BadUSB'],
                cbar=False, annot_kws={"size": 14})
    axes[i].set_title(name, fontsize=14)
    axes[i].set_ylabel('Actual Identity')
    axes[i].set_xlabel('Predicted Identity')

plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.savefig(os.path.join(output_dir, "3_confusion_matrices.png"))
plt.close()

print(f"\nSuccess! All ML comparison graphs saved to: {output_dir}/")

# Print Leaderboard to Terminal
print("\n--- FINAL ML LEADERBOARD ---")
print(results_df.sort_values(by="ROC-AUC", ascending=False).to_string(index=False))