import pandas as pd
import numpy as np

WINDOW_SIZE = 20
LABELS = ["rest", "left", "right", "forward", "backward"]

df = pd.read_csv("merged_imu.csv")

segments = []
segment_labels = []

for label in LABELS:
    gesture = df[df["label"] == label][["gyro_x", "gyro_y", "gyro_z"]].values
    
    if label == "rest":
        # Sample rest periodically to avoid oversampling
        for i in range(0, len(gesture) - WINDOW_SIZE, 4):
            segments.append(gesture[i:i+WINDOW_SIZE])
            segment_labels.append(LABELS.index(label))
    else:
        # Find strongest part of tilt
        mag = np.linalg.norm(gesture, axis=1)
        peak = np.argmax(mag)
        start = max(0, peak - WINDOW_SIZE//2)
        end = start + WINDOW_SIZE
        
        if end <= len(gesture):
            segments.append(gesture[start:end])
            segment_labels.append(LABELS.index(label))

X = np.array(segments, dtype=np.float32)
y = np.array(segment_labels, dtype=np.int32)

# Compute mean / std across entire dataset
mean = X.mean(axis=(0,1), keepdims=True)
std = X.std(axis=(0,1), keepdims=True) + 1e-6

# Normalize
X = (X - mean) / std

np.save("X.npy", X)
np.save("y.npy", y)
np.save("imu_norm_mean.npy", mean)
np.save("imu_norm_std.npy", std)

print("✅ Preprocessing complete:", X.shape, "labels:", y.shape)
