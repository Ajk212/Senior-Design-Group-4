#Author - Aaron Kuchta : 10/2/2025

import sys
import numpy as np
import tensorflow as tf
from tensorflow.keras import layers, models
#import keyboard 
from GloveSettingsBackend import GloveSettingsBackend as Backend
from GloveSettingsFrontend import GloveSettingsFrontend as Frontend
from PyQt5.QtWidgets import QApplication

GESTURES = ["thumbs_up", "thumbs_down"]
NUM_CLASSES = len(GESTURES)

WINDOW_SIZE = 50   #time steps per sample
CHANNELS = 3       #ax ay az
SAMPLES = 500

app = QApplication(sys.argv)
frontend = Frontend()
backend = Backend(frontend)
frontend.show()

def synthesize_data(SAMPLES):
    X, y = [], []
    for i in range(SAMPLES):
        if np.random.rand() < 0.5:
            #thumbs up gesture
            seq = np.ones((WINDOW_SIZE, CHANNELS)) * np.array([1.0, 0.2, 0.5]) #positive values for ax, ay, az
            seq += 0.05*np.random.randn(*seq.shape)  #adds noise to data
            label = 0
        else:
            #thumbs down gesture
            seq = np.ones((WINDOW_SIZE, CHANNELS)) * np.array([-1.0, -0.2, -0.5])
            seq += 0.05*np.random.randn(*seq.shape)
            label = 1
        X.append(seq)
        y.append(label)
    return np.array(X, dtype=np.float32), np.array(y)

X, y = synthesize_data(1000)

#training model
split = int(0.8*len(X))
X_train, X_test = X[:split], X[split:]
y_train, y_test = y[:split], y[split:]

model = models.Sequential([
    layers.Input(shape=(WINDOW_SIZE, CHANNELS)),
    layers.LSTM(32),
    layers.Dense(32, activation='relu'),
    layers.Dense(NUM_CLASSES, activation='softmax')
])

model.compile(optimizer='adam', loss = 'sparse_categorical_crossentropy', metrics=['accuracy'])
model.summary()

model.fit(X_train, y_train, epochs=10, batch_size=32, validation_split=0.2)

loss, acc = model.evaluate(X_test, y_test)
print(f"Test accuracy: {acc:.3f}")



def define_gesture(gesture_id):
    gesture = GESTURES[gesture_id]
    print(f"Gesture Recognized: {gesture}") 
    backend.execute_gesture(gesture)  #call to backend
    #prompt_file_input() #uncomment to allow continuous input

def import_data_from_file(fileName):
    try:
        seq = np.load(fileName)
        x = seq[np.newaxis, ...]
        pred = model.predict(x, verbose=0)
        gesture_id = int(np.argmax(pred))
        define_gesture(gesture_id)
    except FileNotFoundError:
        print(f"File {fileName} not found.")
    
def prompt_file_input():
    print("Please enter the file name containing simulated sensor data: ")
    fileName = input().strip()
    import_data_from_file(fileName)

def create_simulated_data():
    thumbs_up_base = np.array([1.0, 0.2, 0.5])
    thumbs_down_base = np.array([-1.0, -0.2, -0.5])

    up_seq = np.tile(thumbs_up_base, (WINDOW_SIZE, 1)) + np.random.normal(0, .05, (WINDOW_SIZE, CHANNELS))
    down_seq = np.tile(thumbs_down_base, (WINDOW_SIZE, 1)) + np.random.normal(0, .05, (WINDOW_SIZE, CHANNELS))

    np.save("thumbs_up.npy", up_seq)
    np.save("thumbs_down.npy", down_seq)
    print("Simulated data files 'thumbs_up.npy' and 'thumbs_down.npy' created.")

#create_simulated_data()

prompt_file_input()