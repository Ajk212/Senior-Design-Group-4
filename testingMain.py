#Author - Aaron Kuchta : 10/25/2025

import sys
import asyncio
from tensorflow.keras.models import load_model
from PyQt5.QtWidgets import QApplication
from esp32_stream import ESP32BLEClient
from GloveSettingsBackend import GloveSettingsBackend
from GloveSettingsFrontend import GloveSettingsFrontend

async def main():
    app = QApplication(sys.argv)
    frontend = GloveSettingsFrontend()
    backend = GloveSettingsBackend(frontend)
    frontend.show()

    model = load_model('BLE_Test_Model.h5')
    ble = ESP32BLEClient(model, backend)

    await ble.begin_dataStream()

    sys.exit(app.exec_())

if __name__ == "__main__":
    asyncio.run(main())