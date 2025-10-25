#Author - Aaron Kuchta : 10/25/2025

import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models


GESTURES = ["tilt_left", "tilt_right", "tilt_forward", "tilt_backward"]
NUM_CLASSES = len(GESTURES)

WINDOW_SIZE = 50
CHANNELS = 6 #May need to change based off given data


IMU_DATA = {
    "tilt_left":   [-7.3, -0.4, -0.71, -1.5, -5.1, -1.356],
    "tilt_right":  [ 3.3, -0.7,  0.3771, -11, -4.3, 0.556],    
    "tilt_forward":[-4.4, -20.1,  2.82, -4.7, 12, 1.43],
    "tilt_backward":[-1.1, -3.9, -1.12, -7.4, -1.3, -1.856]
}

def generate_data(samples_per_gesture):
    X, y = [], []

    for label, gesture in enumerate(GESTURES):
        baseVal = np.array(IMU_DATA[gesture])
        for i in range(samples_per_gesture):
            seq = np.tile(baseVal, (WINDOW_SIZE, 1))
            seq += np.random.normal(0, .1, seq.shape) #Noise simulation - remove when using better data
            X.append(seq)
            y.append(label)

    return np.array(X, dtype = np.float32), np.array(y)


X, y = generate_data(300)
split = int(.8*len(X))
x_train, x_test = X[:split], X[split:]
y_train, y_test = y[:split], y[split:]

model = models.Sequential([
    layers.Input(shape=(WINDOW_SIZE, CHANNELS)),
    layers.LSTM(32),
    layers.Dense(32, activation='relu'),
    layers.Dense(NUM_CLASSES, activation='softmax')
])

model.compile(optimizer = 'adam', loss = 'sparse_categorical_crossentropy', metrics = ['accuracy'])
model.fit(x_train, y_train, epochs = 12, batch_size = 32, validation_split = .2)

print("\nSaving model as 'BLE_Test_Model.keras'")
model.save('BLE_Test_Model.keras')