#Author - Aaron Kuchta : 10/25/2025

import sys
import asyncio
from qasync import QEventLoop, asyncSlot
import tensorflow as tf
from tensorflow.keras.models import load_model
from PyQt5.QtWidgets import QApplication
from esp32Stream import ESP32BLEClient
from GloveSettingsBackend import GloveSettingsBackend
from GloveSettingsFrontend import GloveSettingsFrontend


async def main():
    
    frontend = GloveSettingsFrontend()
    backend = GloveSettingsBackend(frontend)
    #frontend.show()
    
    model = load_model('BLE_Test_Model.keras')
    ble = ESP32BLEClient(model, backend)

    await ble.begin_dataStream()

if __name__ == "__main__":
    app = QApplication(sys.argv)

    loop = QEventLoop(app)
    asyncio.set_event_loop(loop)

    with loop:
        loop.run_until_complete(main())
    
