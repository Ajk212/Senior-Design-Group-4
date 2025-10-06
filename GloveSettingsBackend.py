import sys
import math
import keyboard

from PyQt5.QtCore import QObject, pyqtSignal
from PyQt5.QtWidgets import QMainWindow, QApplication
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
from comtypes import CLSCTX_ALL

from GloveSettingsFrontend import GloveSettingsFrontend

class GloveSettingsBackend(QObject):
    def __init__(self, frontend):
        super().__init__()
        self.frontend = frontend

        ## Initialize defualt profile 
        # TODO make dynamic profile options
        #self.active_profile = DEFAULT

        self.isConnected = False



    def backendUpdate(self):
        # TODO implement backend update logic
        # TODO update connection status text, connection button, connection routine etc
        pass

    def connectGlove(self):
        # TODO implement connection logic
        pass


    def execute_gesture(self, gesture):
        combo_map = self.frontend.GESTURE_MAP
        if gesture in combo_map:
            action = combo_map[gesture].currentText()
            print(f"Executing action: {action}")

            match action:
                case "Click":
                    pass
                case "Right Click":
                    pass
                case "Minimize Window":
                    keyboard.press_and_release('alt + tab')
                case "Play/Pause Media":
                    pass
                case "Next Media":
                    pass
                case "Previous Media":
                    pass
                case "Scroll Up":
                    pass
                case "Scroll Down":
                    pass
                case "Volume Up":
                    self.execute_volume_change(.05)
                case "Volume Down":
                    self.execute_volume_change(-.05)
        else:
            print(f"Gesture {gesture} not recognized.")


    def execute_volume_change(self, step):
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        volume = interface.QueryInterface(IAudioEndpointVolume)

        curr = volume.GetMasterVolumeLevelScalar()
        new = curr + step
        volume.SetMasterVolumeLevelScalar(new, None)



if __name__ == "__main__":
    app = QApplication(sys.argv)  
    frontend = GloveSettingsFrontend()
    backend = GloveSettingsBackend(frontend)
    frontend.show()
    sys.exit(app.exec_()) 