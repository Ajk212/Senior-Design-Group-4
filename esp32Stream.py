#Author - Aaron Kuchta : 10/25/2025

import struct
import numpy as np
import asyncio
from bleak import BleakScanner, BleakClient
from collections import deque

GESTURES = ["tilt_left", "tilt_right", "tilt_forward", "tilt_backward"]

WINDOW_SIZE = 30
CHANNELS = 6

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
        self.accel1_x, self.accel1_y, self.accel1_z = unpacked[0:3]
        self.gyro1_x, self.gyro1_y, self.gyro1_z = unpacked[3:6]
        self.temp1 = unpacked[6]
        
        self.accel2_x, self.accel2_y, self.accel2_z = unpacked[7:10]
        self.gyro2_x, self.gyro2_y, self.gyro2_z = unpacked[10:13]
        self.temp2 = unpacked[13]
        
        self.timestamp = unpacked[14]
        self.frame_count = unpacked[15]
        self.status = unpacked[16] 




class ESP32BLEClient:
    def __init__(self, model, backend):
        self.model = model
        self.backend = backend
        self.window = deque(maxlen = WINDOW_SIZE)
        self.client = None
        self.connection_established = False

    async def establish_connection(self):
        print("Searching for ESP32 device")
        device = await BleakScanner.discover()
        for d in device:
            if d.name and "ESP32C6-HANDSENSE" in d.name:
                self.client = BleakClient(d.address)
                await self.client.connect()
                print(f"Connected to {d.name}")
                self.connection_established = True
                await self.client.start_notify(CHAR_UUID, self.imu_notification_handler)
                return
        print("ESP32 device not found")


    def received_data(self, sender, data):
        try:
            values = np.array([float(v) for v in data.decode().strip().split(',')])
            if len(values) == CHANNELS:
                self.window.append(values)
        except:
            print("Error parsing data")
            return
        
        if len(self.window) == WINDOW_SIZE:
            x = np.array(self.window)[np.newaxis, ...]
            prediction = self.model.predict(x, verbose=0)
            gesture_id = int(np.argmax(prediction))
            self.backend.execute_gesture(gesture_id)

    async def begin_dataStream(self):
        while not self.connection_established:
            await self.establish_connection()
        while True:
            await asyncio.sleep(.1)


    async def enable_notifications(self):
        if not self.connected:
            return False
            
        try:
            # Start notifications
            await self.client.start_notify(CHAR_UUID, self.imu_notification_handler)
            print(f"Enabled notifications for {CHAR_UUID}")
            return True
        except Exception as e:
            print(f"Failed to enable notifications: {e}")
            return False
    
    def imu_notification_handler(self, sender, data):
        frame = IMUFrame(data)
        gyro_values = [
            frame.gyro1_x, frame.gyro1_y, frame.gyro1_z,
            frame.gyro2_x, frame.gyro2_y, frame.gyro2_z
        ]
        self.window.append(gyro_values)

        
        if len(self.window) == WINDOW_SIZE:
            x = np.array(self.window)[np.newaxis, ...]
            prediction = self.model.predict(x, verbose=0)
            gesture_id = int(np.argmax(prediction))
            gesture_name = GESTURES[gesture_id]
            print(f"Predicted Gesture: {gesture_name}")
    '''
    def imu_notification_handler(self, sender, data):
        frame = IMUFrame(data)
        print(f"\n IMU Frame #{frame.frame_count}:")
        print(f"   IMU2 - Gyro:  ({frame.gyro2_x:6.2f}, {frame.gyro2_y:6.2f}, {frame.gyro2_z:6.2f}) °/s")

        print(f"   Metadata - Timestamp: {frame.timestamp}")
        print(f"              Status: {frame.status}")
    '''