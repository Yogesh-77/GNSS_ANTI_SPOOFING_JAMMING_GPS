import pandas as pd
import numpy as np
from xgboost import XGBClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
import joblib

print("Loading datasets...")

columns = [
    "time",
    "lat",
    "lon",
    "speed_kmph",
    "numSV",
    "hdop"
]

normal = pd.read_csv("normal_gps.csv", names=columns)
jam = pd.read_csv("jam_gps.csv", names=columns)

normal["label"] = 0
jam["label"] = 1

data = pd.concat([normal, jam])

print("Creating advanced AI features...")

data["speed_change"] = data["speed_kmph"].diff()
data["sat_drop"] = data["numSV"].diff()
data["hdop_change"] = data["hdop"].diff()

data["position_jump"] = np.sqrt(
    data["lat"].diff()**2 +
    data["lon"].diff()**2
)

data = data.fillna(0)

features = [
    "lat",
    "lon",
    "speed_kmph",
    "numSV",
    "hdop",
    "speed_change",
    "sat_drop",
    "hdop_change",
    "position_jump"
]

X = data[features]
y = data["label"]

print("Training Advanced XGBoost Model...")

X_train,X_test,y_train,y_test = train_test_split(
    X,y,test_size=0.2,random_state=42
)

model = XGBClassifier(
    n_estimators=400,
    max_depth=8,
    learning_rate=0.03,
    subsample=0.9
)

model.fit(X_train,y_train)

pred = model.predict(X_test)

print("Accuracy:", accuracy_score(y_test,pred))

joblib.dump(model,"advanced_gnss_model.pkl")

print("Model saved successfully!")