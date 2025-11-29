import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras import layers, models
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
import os
import glob

#Directory containing training data
DIR = "gesture_datasets"

NUM_GESTURES = 13
SEQ_LENGTH = 20

#MUST MATCH ESP32 FRAME
FEATURE_COLUMNS = [
    "accel1_x", "accel1_y", "accel1_z",
    "gyro1_x", "gyro1_y", "gyro1_z",
    "accel2_x", "accel2_y", "accel2_z",
    "gyro2_x", "gyro2_y", "gyro2_z",
    "flex1", "flex2", "flex3", "flex4", "flex5"
]
LABEL_COLUMN = "gesture_label"
BATCH_SIZE = 32
EPOCHS = 20

def load_data(directory):
    csv_files = glob.glob(os.path.join(directory, "*.csv"))
    print(f"Found {len(csv_files)} CSV files.")

    data = []
    for file in csv_files:
        try:
            df = pd.read_csv(file)
            data.append(df)
        except Exception as e:
            print(f"Error reading {file}: {e}")

    combined_data = pd.concat(data, ignore_index=True)
    return combined_data

def process_data(data, labels, win_size):
    X, y = [], []

    for i in range(len(data) - win_size + 1):
        X.append(data[i:i + win_size])
        y.append(labels[i + win_size - 1])

    return np.array(X), np.array(y)

print("Now loading data...")
df = load_data(DIR)

missing_cols = set(FEATURE_COLUMNS + [LABEL_COLUMN]) - set(df.columns)
if missing_cols:
    print(f"Warning: Missing columns: {missing_cols}")
    print(f"Available columns: {df.columns.tolist()}")
    exit(1)


X_data = df[FEATURE_COLUMNS].values
y_data = df[LABEL_COLUMN].values

label_encoder = LabelEncoder()
y_encoded = label_encoder.fit_transform(y_data)
NUM_CLASSES = len(label_encoder.classes_)

print(f"\n=== Data Summary ===")
print(f"Unique gestures: {label_encoder.classes_}")
print(f"Number of classes: {NUM_CLASSES}")
print(f"Total samples: {len(X_data)}")

X, y = process_data(X_data, y_encoded, SEQ_LENGTH)

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, stratify=y
)

print("Now building model...")
#Uses 1D Convolution instead of LSTM (Smaller size)
model = models.Sequential([
    layers.Input(shape=(SEQ_LENGTH, len(FEATURE_COLUMNS))),
    
    layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
    layers.Conv1D(32, kernel_size=3, activation='relu', padding='same'),
    layers.MaxPooling1D(pool_size=2),
    layers.Dropout(0.3),
    
    layers.Conv1D(64, kernel_size=3, activation='relu', padding='same'),
    layers.GlobalAveragePooling1D(),
    layers.Dropout(0.3),
    
    layers.Dense(64, activation='relu'),
    layers.Dropout(0.3),
    layers.Dense(32, activation='relu'),
    layers.Dense(NUM_CLASSES, activation='softmax')
])

model.compile(
    optimizer = 'adam',
    loss = 'sparse_categorical_crossentropy',
    metrics = ['accuracy']
)

model.summary()

print("Now training model...")
history = model.fit(
    X_train, y_train,
    epochs = EPOCHS,
    batch_size = BATCH_SIZE,
    validation_data = (X_test, y_test),
    verbose = 1
)

print("\n=== Evaluation ===")
test_loss, test_accuracy = model.evaluate(X_test, y_test, verbose=0)
print(f"Test Loss: {test_loss:.4f}")
print(f"Test Accuracy: {test_accuracy:.4f}")

#Update to set filename
model.save("final_model_30SamplesV2.keras")
print("Model saved as final_model_30SamplesV2.keras")