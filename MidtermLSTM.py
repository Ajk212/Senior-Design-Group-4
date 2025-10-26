#Author - Aaron Kuchta : 10/25/2025

import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models

WINDOW_SIZE = 20
CHANNELS = 3 #May need to change based off given data


X = np.load("X.npy")
y = np.load("y.npy")

NUM_CLASSES = 5


model = models.Sequential([
    layers.Input(shape=(WINDOW_SIZE, CHANNELS)),
    layers.LSTM(48),
    layers.Dense(32, activation='relu'),
    layers.Dense(NUM_CLASSES, activation='softmax')
])

model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
model.fit(X, y, epochs=20, batch_size=32, validation_split=0.2)

print("\nSaving model as 'BLE_Test_Model.keras'")
model.save('BLE_Test_Model.keras')