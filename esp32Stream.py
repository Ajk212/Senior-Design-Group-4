#Author - Aaron Kuchta : 10/25/2025

import struct
import numpy as np
import asyncio
from bleak import BleakScanner, BleakClient
from collections import deque

LABELS = ["rest", "tilt_left", "tilt_right", "tilt_forward", "tilt_backward"]
mean = np.load("imu_norm_mean.npy")
std = np.load("imu_norm_std.npy")

WINDOW_SIZE = 20
CHANNELS = 3

SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"  # Service 0x00FF
CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"     # Characteristic 0xFF01
CCC_DESCRIPTOR_UUID = "00002902-0000-1000-8000-00805f9b34fb"  # CCC Descriptor
CHAR_A_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"   
IMU_FRAME_FORMAT = '<14fLLb' 
IMU_FRAME_SIZE = 14*4 + 2*4 + 1 

class IMUFrame:
    def __init__(self, data):
        if len(data) != IMU_FRAME_SIZE:
            print(f"ERROR: Expected {IMU_FRAME_SIZE} bytes, got {len(data)} bytes. Raw data: {data.hex()}")
            # Raise an error or return early if data size is wrong.
            raise struct.error(f"IMU unpack requires a buffer of {IMU_FRAME_SIZE} bytes, but received {len(data)}")

        # Unpack 14 floats, 2 uint32s, 1 uint8
        unpacked = struct.unpack(IMU_FRAME_FORMAT, data)
        self.gyro1_x, self.gyro1_y, self.gyro1_z = unpacked[7:10]


class ESP32BLEClient:
    def __init__(self, model, backend):
        self.model = model
        self.backend = backend
        self.window = deque(maxlen = WINDOW_SIZE)
        self.client = None
        self.connection_established = False

        self.last_gesture = "rest"

    async def establish_connection(self):
        print("Searching for ESP32 device")
        device = await BleakScanner.discover()
        for d in device:
            if d.name and "ESP32C6-HANDSENSE" in d.name:
                self.client = BleakClient(d.address)
                await self.client.connect()
                print(f"Connected to {d.name}")
                self.connection_established = True

                await asyncio.sleep(3.0)
                
                await self.client.start_notify(CHAR_UUID, self.imu_notification_handler)
                return
        print("ESP32 device not found")

    async def begin_dataStream(self):
        while not self.connection_established:
            await self.establish_connection()
        while True:
            await asyncio.sleep(.1)
    
    def imu_notification_handler(self, sender, data):
        frame = IMUFrame(data)

        gyro_values = np.array([frame.gyro1_x, frame.gyro1_y, frame.gyro1_z], dtype=np.float32)
        self.window.append(gyro_values)

        if len(self.window) < WINDOW_SIZE:
            return

        x = np.array(self.window, dtype=np.float32)[None, :, :]
        x = (x - mean) / std

        prediction = self.model.predict(x, verbose=0)[0]
        gesture_id = int(np.argmax(prediction))
        confidence = prediction[gesture_id]

        CONF_THRESHOLD = 0.60

        if confidence < CONF_THRESHOLD:
            gesture_name = "rest"
        else:
            gesture_name = LABELS[gesture_id]
        
        #Smoothing
        if gesture_name != self.last_gesture:
            #Require stronger confidence to switch gestures
            if confidence < 0.70:
                gesture_name = self.last_gesture

        self.last_gesture = gesture_name
        

        print(f"Predicted: {gesture_name} (conf={confidence:.2f})")

        self.backend.execute_gesture(gesture_name)

    '''

    def imu_notification_handler(self, sender, data):
        frame = IMUFrame(data)
        #print(f"\n IMU Frame #{frame.frame_count}:")
        print(f"   IMU2 - Gyro:  ({frame.accel2_x:6.2f}, {frame.accel2_y:6.2f}, {frame.accel2_z:6.2f}) °/s")

        #print(f"   Metadata - Timestamp: {frame.timestamp}")
        #print(f"              Status: {frame.status}")
        
    '''