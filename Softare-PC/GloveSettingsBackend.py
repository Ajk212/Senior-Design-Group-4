import sys
import math
import keyboard
import pyautogui
import asyncio

from PyQt5.QtCore import QObject, pyqtSignal
from PyQt5.QtWidgets import QMainWindow, QApplication
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
from comtypes import CLSCTX_ALL
from PyQt5.QtCore import pyqtSignal

from bleak import BleakScanner, BleakClient
CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"     # Characteristic 0xFF01


from GloveSettingsFrontend import GloveSettingsFrontend

class GloveSettingsBackend(QObject):
    def __init__(self, frontend):
        super().__init__()
        self.frontend = frontend

        ## Initialize defualt profile 
        # TODO make dynamic profile options
        #self.active_profile = DEFAULT

        self.isConnected = False
        self.haltExecution = True

        self.MODES = ["Navigation", "Input", "Paused"]

        self.currentMode = self.MODES[0]

    def backendUpdate(self):
        # TODO implement backend update logic
        # TODO update connection status text, connection button, connection routine etc
        pass

    async def connectGlove(self):
        print("Searching for HandSense Device...")

        device = await BleakScanner.discover()
        for d in device:
            if d.name and "ESP32C6-HANDSENSE" in d.name:
                self.client = BleakClient(d.address)
                await self.client.connect()
                print(f"Connected to {d.name}")
                self.isConnected = True

                await asyncio.sleep(3.0)

                await self.client.start_notify(CHAR_UUID, self.recieve_detected_gesture)

    def recieve_detected_gesture(self, gesture):
        # TODO Test recieving gesture from glove
        pass

    def execute_gesture(self, gesture):
        if self.haltExecution:
            self.haltExecution = True
            combo_map = self.frontend.GESTURE_MAP
            if gesture == "tilt_left" or gesture == "tilt_right":
                action = gesture
                #action = combo_map[gesture].currentText()
                #print(f"Executing action: {action}")
                #if self.currentMode == "Paused" & action != "Pressure Sensor Held":
                #    print("Gesture execution paused.")
                #    self.haltExecution = False
                #    return
                
                #print(f"Executing action: {action}")

                match action:
                    case "Click":
                        pyautogui.click()
                    case "Right Click":
                        pyautogui.rightClick()
                    case "Pressure Sensor Held":
                        self.change_mode()
                    case "Minimize Window":
                        keyboard.press_and_release('win + down')
                    case "Play/Pause Media":
                        keyboard.press_and_release('space')
                    case "Next Media":
                        keyboard.press_and_release('right') #TODO fix next media keybind
                    case "Previous Media":
                        keyboard.press_and_release('left') #TODO fix previous media keybind
                    case "Scroll Up":
                        pyautogui.scroll(500)
                    case "Scroll Down":
                        pyautogui.scroll(-500)
                    case "Volume Up":
                        self.execute_volume_change(.05)
                    case "Volume Down":
                        self.execute_volume_change(-.05)
                        #case move_mouse_up:
                        #case move_mouse_down:
                        #case move_mouse_left:
                        #case move_mouse_right:
                        #case 
            else:
                #print(f"Gesture {gesture} not recognized.")
                pass

            #self.send_ACK_to_glove()
            #self.haltExecution = False

    def change_mode(self):
        current_index = self.MODES.index(self.currentMode)
        next_index = (current_index + 1) % len(self.MODES)
        self.currentMode = self.MODES[next_index]
        print(f"Mode changed to: {self.currentMode}")

    def execute_volume_change(self, step):
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        volume = interface.QueryInterface(IAudioEndpointVolume)

        curr = volume.GetMasterVolumeLevelScalar()
        new = max(0.0, min(1.0, curr + step))
        volume.SetMasterVolumeLevelScalar(new, None)

    def send_ACK_to_glove():
        #TODO send acknowledgement to glove and pulse haptic motor
        pass

if __name__ == "__main__":
    app = QApplication(sys.argv)  
    frontend = GloveSettingsFrontend()
    backend = GloveSettingsBackend(frontend)

    frontend.connect_requested.connect(backend.connectGlove)

    frontend.show()
    sys.exit(app.exec_())