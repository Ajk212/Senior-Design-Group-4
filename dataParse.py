import numpy as np

WINDOW_SIZE = 20
SAMPLES_PER_CLASS = 500  # More samples = better generalization

LABELS = ["rest", "tilt_left", "tilt_right", "tilt_forward", "tilt_backward"]

def make_windows(center, spread, count):
    base = np.array(center, dtype=np.float32)
    return base + np.random.normal(0, spread, size=(count, WINDOW_SIZE, 3))

segments = []
labels = []

# REST: gentle noise near zero
segments.append(make_windows([0.0, 0.0, 0.0], 1.8, SAMPLES_PER_CLASS))
labels += [LABELS.index("rest")] * SAMPLES_PER_CLASS

# LEFT: gyro_y slightly below -3
segments.append(make_windows([0.0, -4.7, 0.0], 0.5, SAMPLES_PER_CLASS))
labels += [LABELS.index("tilt_left")] * SAMPLES_PER_CLASS

# RIGHT: gyro_y slightly above +3
segments.append(make_windows([0.0, +4.7, 0.0], 0.5, SAMPLES_PER_CLASS))
labels += [LABELS.index("tilt_right")] * SAMPLES_PER_CLASS

# FORWARD: gyro_x slightly above +3
segments.append(make_windows([+4.7, 0.0, 0.0], 0.5, SAMPLES_PER_CLASS))
labels += [LABELS.index("tilt_forward")] * SAMPLES_PER_CLASS

# BACKWARD: gyro_x slightly below -3
segments.append(make_windows([-4.7, 0.0, 0.0], 0.5, SAMPLES_PER_CLASS))
labels += [LABELS.index("tilt_backward")] * SAMPLES_PER_CLASS

X = np.vstack(segments)
y = np.array(labels, dtype=np.int32)

# Normalize (must match runtime)
mean = X.mean(axis=(0,1), keepdims=True)
std = X.std(axis=(0,1), keepdims=True) + 1e-6
X = (X - mean) / std

np.save("X.npy", X)
np.save("y.npy", y)
np.save("imu_norm_mean.npy", mean)
np.save("imu_norm_std.npy", std)

print("✅ New Training Data Created:", X.shape, y.shape)

