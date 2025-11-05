import tensorflow as tf
import os


model_path = os.path.join(os.path.dirname(__file__), "BLE_Test_Model.keras")
model = tf.keras.models.load_model(model_path)


converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()


with open("navigation_model_test.tflite", "wb") as f:
    f.write(tflite_model)