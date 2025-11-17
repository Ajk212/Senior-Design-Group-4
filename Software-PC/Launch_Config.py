#Author - Aaron Kuchta : 10/25/2025

import sys
import asyncio
from qasync import QEventLoop, asyncSlot
import tensorflow as tf
from PyQt5.QtWidgets import QApplication
from GloveSettingsBackend import GloveSettingsBackend
from GloveSettingsFrontend import GloveSettingsFrontend


if __name__ == "__main__":

    app = QApplication(sys.argv)  

    frontend = GloveSettingsFrontend()
    backend = GloveSettingsBackend(frontend)

    loop = QEventLoop(app)
    asyncio.set_event_loop(loop)

    frontend.connect_requested.connect(backend.connectGlove)
    backend.connection_status_changed.connect(frontend.updateConnectionStatus)
 

    frontend.show()

    try:
        with loop:
            loop.run_forever()
    except KeyboardInterrupt:
        print("Application closed by user.")
        loop.stop()
        app.quit()

    #with loop:
    #    loop.run_forever()


    
