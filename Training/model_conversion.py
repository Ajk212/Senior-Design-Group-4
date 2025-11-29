import tensorflow as tf
import os
import subprocess

#Must match model name
model_path = "final_model_30SamplesV2.keras"
tflite_path = "final_model_30SamplesV2.tflite"

print("Loading model")
model = tf.keras.models.load_model(model_path)
model.summary()

print("\nNow converting file")

converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

#Savve model
with open(tflite_path, "wb") as f:
    f.write(tflite_model)

print(f"TFLite model saved to: {tflite_path}")

#To convert to ESP32 usable .h file, use xxd command via terminal