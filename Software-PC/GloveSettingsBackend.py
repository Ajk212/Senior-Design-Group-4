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
SERVICE_UUID = "000000ff-0000-1000-8000-00805f9b34fb"  # 0x00FF 
TX_UUID      = "0000ff01-0000-1000-8000-00805f9b34fb"  # 0xFF01 (read/notify)
RX_UUID      = "0000ff02-0000-1000-8000-00805f9b34fb"  # 0xFF02 (write)
CCCD_UUID     = "00002902-0000-1000-8000-00805f9b34fb"  # CCCD

from GloveSettingsFrontend import GloveSettingsFrontend

class GloveSettingsBackend(QObject):
    connection_status_changed = pyqtSignal(bool)

    def __init__(self, frontend):
        super().__init__()
        self.frontend = frontend

        self.isConnected = False
        self.haltExecution = True

        self.MODES = ["Navigation", "Input", "Paused"]

        self.currentMode = self.MODES[0]


    def connectGlove(self):
        asyncio.create_task(self.connectGloveAsync())

    async def connectGloveAsync(self):
        print("Searching for HandSense Device...")

        device = await BleakScanner.discover()
        for d in device:
            if d.name and "ESP32C6-HANDSENSE" in d.name:
                self.client = BleakClient(d.address)
                try:
                    await self.client.connect()
                    print(f"Connected to {d.name}")
                    self.isConnected = True
                    self.connection_status_changed.emit(self.isConnected)

                    await asyncio.sleep(2.0)

                    await self.client.start_notify(TX_UUID, self.recieve_detected_gesture)
                    print("Notification handler started.")

                    while await self.client.is_connected():
                        await asyncio.sleep(1.0)

                except Exception as e:
                    print(f"Failed to connect to device: {e}")

                print("Device disconnected")
                self.isConnected = False
                self.connection_status_changed.emit(self.isConnected)
                await self.attempt_reconnect()
            
            
    def recieve_detected_gesture(self, sender, gesture):
        print(f"Received gesture: {gesture.decode('utf-8')}")
        gesture = gesture.decode('utf-8').strip()
        self.execute_gesture(gesture)
    

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
                    case "tilt_right":
                        self.execute_volume_change(.05)
                    case "tilt_left":
                        self.execute_volume_change(-.05)
                    case "mouse_up":
                        pass
                    case "mouse_down":
                        pass
                    case "mouse_left":
                        pass
                    case "mouse_right":
                        pass
                    
            else:
                print(f"Gesture {gesture} not recognized.")

            asyncio.create_task(self.send_ACK_to_glove())

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

    async def send_ACK_to_glove(self):
        if self.isConnected:
            try:
                asyncio.create_task(self.client.write_gatt_char(RX_UUID, b"ACK"))
                print("ACK sent to glove.")
            except Exception as e:
                print(f"Failed to send ACK: {e}")
            

    def on_disconnect(self, client):
        print("Device disconnected")
        self.isConnected = False
        self.connection_status_changed.emit(self.isConnected)

    async def attempt_reconnect(self):
        while not self.isConnected:
            print("Attempting to reconnect...")
            try:
                await self.client.connect()
                print("Reconnected successfully")
                self.isConnected = True
                self.connection_status_changed.emit(self.isConnected)
                await self.client.start_notify(TX_UUID, self.recieve_detected_gesture)
            except Exception as e:
                print(f"Reconnection failed: {e}")
                await asyncio.sleep(5) 

