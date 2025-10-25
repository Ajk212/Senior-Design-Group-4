#Author - Aaron Kuchta : 10/25/2025

import numpy as np
import asyncio
from bleak import BleakScanner, BleakClient
from collections import deque

WINDOW_SIZE = 50
CHANNELS = 6 

CHAR_A_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"   

class ESP32BLEClient:
    def __init__(self, model, backend):
        self.model = model
        self.backend = backend
        self.window = deque(maxlen = WINDOW_SIZE)
        self.client = None

    async def establish_connection(self):
        print("Searching for ESP32 device")
        device = await BleakScanner.discover()
        for d in device:
            if d.name and "ESP_GATTS_DEMO" in d.name:
                self.client = BleakClient(d.address)
                await self.client.connect()
                print(f"Connected to {d.name}")
                await self.client.start_notify(CHAR_A_UUID, self.received_data)
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
        await self.establish_connection()
        print("Now receiving data from ESP32")
        while True:
            await asyncio.sleep(.1)