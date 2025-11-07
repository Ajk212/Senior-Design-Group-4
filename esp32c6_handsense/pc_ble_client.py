import struct
import asyncio
import time
from bleak import BleakScanner, BleakClient

# ESP32 UUIDs for IMU Service
IMU_SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"  # Service 0x00FF
IMU_CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"     # Characteristic 0xFF01
IMU_CCC_DESCRIPTOR_UUID = "00002902-0000-1000-8000-00805f9b34fb"  # CCC Descriptor

# ESP32 UUIDs for Gesture Service  
GESTURE_SERVICE_UUID = "0000fe00-0000-1000-8000-00805f9b34fb"  # Service 0x00FE
GESTURE_CHAR_UUID = "0000fe01-0000-1000-8000-00805f9b34fb"     # Characteristic 0xFE01
ACK_CHAR_UUID = "0000fe02-0000-1000-8000-00805f9b34fb"         # Characteristic 0xFE02

IMU_FRAME_FORMAT = '<14fLLb'  # Little-endian, 14 floats, 2 unsigned long, 1 char - Total 17 fields
IMU_FRAME_SIZE = 14*4 + 2*4 + 1  # 14 floats * 4 bytes + 2 uint32 * 4 bytes + 1 uint8 = 65 bytes

class IMUFrame:
    def __init__(self, data):
        if len(data) != IMU_FRAME_SIZE:
            print(f"ERROR: Expected {IMU_FRAME_SIZE} bytes, got {len(data)} bytes. Raw data: {data.hex()}")
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

def imu_notification_handler(sender, data):
    try:
        frame = IMUFrame(data)
        print(f"\n IMU Frame #{frame.frame_count}:")
        print(f"   IMU1 - Accel: ({frame.accel1_x:6.2f}, {frame.accel1_y:6.2f}, {frame.accel1_z:6.2f}) m/s²")
        print(f"          Gyro:  ({frame.gyro1_x:6.2f}, {frame.gyro1_y:6.2f}, {frame.gyro1_z:6.2f}) °/s")
        print(f"          Temp:  {frame.temp1:5.1f}°F")
        print(f"   IMU2 - Accel: ({frame.accel2_x:6.2f}, {frame.accel2_y:6.2f}, {frame.accel2_z:6.2f}) m/s²")
        print(f"          Gyro:  ({frame.gyro2_x:6.2f}, {frame.gyro2_y:6.2f}, {frame.gyro2_z:6.2f}) °/s")
        print(f"          Temp:  {frame.temp2:5.1f}°F")
        print(f"   Metadata - Timestamp: {frame.timestamp}")
        print(f"              Status: {frame.status}")
    except Exception as e:
        print(f"Error parsing IMU data: {e}")

class ESP32BLEClient:
    def __init__(self):
        self.client = None
        self.connected = False
        self.gesture_callback = None
        
    # Handle incoming gesture notifications from ESP32
    def gesture_notification_handler(self, sender, data):
        try:
            gesture = data.decode('utf-8', errors='ignore').strip()
            print(f"🎯 Received gesture: {gesture}")
            
            # Execute the gesture and send ACK
            asyncio.create_task(self.execute_gesture(gesture))
        except Exception as e:
            print(f"Error processing gesture: {e}")
    
    # Handle ACK notifications (if needed)
    def ack_notification_handler(self, sender, data):
        try:
            ack_message = data.decode('utf-8', errors='ignore').strip()
            print(f"📨 ACK notification: {ack_message}")
        except:
            print(f"ACK notification raw: {data.hex()}")
    
    # Execute gesture and send ACK back to ESP32
    async def execute_gesture(self, gesture):
        print(f"🤖 Executing gesture: {gesture}")
        
        # Simulate gesture execution
        await asyncio.sleep(0.5)
        
        # Send ACK back to ESP32
        ack_message = "OK"
        
        # You can customize ACK based on gesture success
        if "FAIL" in gesture.upper() or "ERROR" in gesture.upper():
            ack_message = "FAILED"
        elif "CALIBRATE" in gesture.upper():
            ack_message = "CALIBRATION_DONE"
        elif "START" in gesture.upper():
            ack_message = "STARTED"
        elif "STOP" in gesture.upper():
            ack_message = "STOPPED"
            
        await self.send_ack(ack_message)
        print(f"✅ Sent ACK: {ack_message} for gesture: {gesture}")
    
    # Send ACK to ESP32
    async def send_ack(self, ack_message):
        if not self.connected:
            return False
            
        try:
            if isinstance(ack_message, str):
                ack_message = ack_message.encode('utf-8')
                
            await self.client.write_gatt_char(ACK_CHAR_UUID, ack_message, response=True)
            return True
        except Exception as e:
            print(f"Failed to send ACK: {e}")
            return False
    
    # Send gesture command to ESP32
    async def send_gesture(self, gesture_command):
        if not self.connected:
            return False
            
        try:
            if isinstance(gesture_command, str):
                gesture_command = gesture_command.encode('utf-8')
                
            await self.client.write_gatt_char(GESTURE_CHAR_UUID, gesture_command, response=True)
            print(f"📤 Sent gesture: {gesture_command.decode('utf-8')}")
            return True
        except Exception as e:
            print(f"Failed to send gesture: {e}")
            return False

    # Scan and connect to ESP32
    async def connect_to_esp32(self):
        print("Scanning for ESP32...")
        devices = await BleakScanner.discover()
        esp32_device = None
        
        for device in devices:
            if device.name and "ESP32C6-HANDSENSE" in device.name:
                esp32_device = device
                print(f" Found: {device.name} - {device.address}")
                break
        
        if not esp32_device:
            print("ESP32 not found!")
            return False
            
        # Connect to device
        self.client = BleakClient(esp32_device.address, disconnected_callback=self.disconnect_handler)
        await self.client.connect()
        self.connected = True
        print(f"Connected to {esp32_device.name}")
        return True
        
    # Handle disconnection
    def disconnect_handler(self, client):
        print("Disconnected from ESP32")
        self.connected = False
        
    # Explore all services and characteristics
    async def explore_services(self):
        if not self.connected:
            return
            
        services = self.client.services
        print("\n Services Discovered:")
        for service in services:
            print(f"Service: {service.uuid} - {service.description}")
            for char in service.characteristics:
                print(f"  Characteristic: {char.uuid}")
                print(f"  Properties: {char.properties}")
                if char.descriptors:
                    for desc in char.descriptors:
                        print(f"    Descriptor: {desc.uuid}")
                
    # Read from characteristic
    async def read_characteristic(self, char_uuid):
        if not self.connected:
            return
            
        try:
            value = await self.client.read_gatt_char(char_uuid)
            print(f"Characteristic ({char_uuid}): {value.hex()} -> {value.decode('utf-8', errors='ignore')}")
        except Exception as e:
            print(f"  Failed to read characteristic {char_uuid}: {e}")
            
    # Enable IMU notifications
    async def enable_imu_notifications(self):
        if not self.connected:
            return False
            
        try:
            await self.client.start_notify(IMU_CHAR_UUID, imu_notification_handler)
            
            # Enable notifications by writing to CCC descriptor
            for service in self.client.services:
                for char in service.characteristics:
                    if char.uuid == IMU_CHAR_UUID:
                        for descriptor in char.descriptors:
                            if descriptor.uuid == IMU_CCC_DESCRIPTOR_UUID:
                                await self.client.write_gatt_descriptor(descriptor.handle, b'\x01\x00')
                                print(f"✅ Enabled IMU notifications")
                                return True
            return False
        except Exception as e:
            print(f"Failed to enable IMU notifications: {e}")
            return False
            
    # Enable gesture notifications
    async def enable_gesture_notifications(self):
        if not self.connected:
            return False
            
        try:
            await self.client.start_notify(GESTURE_CHAR_UUID, self.gesture_notification_handler)
            print(f"✅ Enabled gesture notifications")
            return True
        except Exception as e:
            print(f"Failed to enable gesture notifications: {e}")
            return False

    # Disable notifications
    async def disable_notifications(self, char_uuid):
        if not self.connected:
            return
            
        try:
            await self.client.stop_notify(char_uuid)
            print(f"Disabled notifications for {char_uuid}")
        except Exception as e:
            print(f"Failed to disable notifications: {e}")

    # Interactive mode for sending/receiving data
    async def interactive_mode(self):
        if not self.connected:
            return
            
        print("\n Interactive Mode - Type commands:")
        print("  'imu on/off' - Enable/disable IMU notifications")
        print("  'gesture on/off' - Enable/disable gesture notifications")  
        print("  'send <gesture>' - Send gesture command to ESP32")
        print("  'ack <message>' - Send ACK message to ESP32")
        print("  'read imu/gesture/ack' - Read characteristic values")
        print("  'services' - List all services")
        print("  'quit' - Exit")
        
        while self.connected:
            try:
                command = input("\n>>> ").strip().lower()
                
                if command == 'quit':
                    break
                elif command == 'services':
                    await self.explore_services()
                elif command.startswith('read'):
                    parts = command.split()
                    if len(parts) >= 2:
                        char_type = parts[1].lower()
                        if char_type == 'imu':
                            await self.read_characteristic(IMU_CHAR_UUID)
                        elif char_type == 'gesture':
                            await self.read_characteristic(GESTURE_CHAR_UUID)
                        elif char_type == 'ack':
                            await self.read_characteristic(ACK_CHAR_UUID)
                        else:
                            print("Usage: read <imu|gesture|ack>")
                    else:
                        print("Usage: read <imu|gesture|ack>")
                        
                elif command.startswith('imu'):
                    parts = command.split()
                    if len(parts) >= 2:
                        action = parts[1].lower()
                        if action == 'on':
                            await self.enable_imu_notifications()
                        elif action == 'off':
                            await self.disable_notifications(IMU_CHAR_UUID)
                        else:
                            print("Use 'on' or 'off'")
                    else:
                        print("Usage: imu <on/off>")
                        
                elif command.startswith('gesture'):
                    parts = command.split()
                    if len(parts) >= 2:
                        action = parts[1].lower()
                        if action == 'on':
                            await self.enable_gesture_notifications()
                        elif action == 'off':
                            await self.disable_notifications(GESTURE_CHAR_UUID)
                        else:
                            print("Use 'on' or 'off'")
                    else:
                        print("Usage: gesture <on/off>")
                        
                elif command.startswith('send'):
                    parts = command.split()
                    if len(parts) >= 2:
                        gesture_text = ' '.join(parts[1:])
                        await self.send_gesture(gesture_text)
                    else:
                        print("Usage: send <gesture_command>")
                        
                elif command.startswith('ack'):
                    parts = command.split()
                    if len(parts) >= 2:
                        ack_text = ' '.join(parts[1:])
                        await self.send_ack(ack_text)
                    else:
                        print("Usage: ack <ack_message>")
                else:
                    print("Unknown command")
                    
            except Exception as e:
                print(f"Error: {e}")

    # Run a demo sequence automatically
    async def demo_sequence(self):
        if not self.connected:
            return
            
        print("\n Starting Demo Sequence...")
        
        # 1. Explore services
        await self.explore_services()
        
        # 2. Enable IMU notifications
        print("\n1. Enabling IMU notifications...")
        await self.enable_imu_notifications()
        
        # 3. Enable gesture notifications  
        print("\n2. Enabling gesture notifications...")
        await self.enable_gesture_notifications()
        
        # 4. Send some test gestures
        test_gestures = [
            "FIST",
            "WAVE", 
            "THUMBS_UP",
            "OPEN_HAND",
            "CALIBRATE_SENSORS",
            "START_RECORDING"
        ]
        
        print("\n3. Sending test gestures...")
        for i, gesture in enumerate(test_gestures):
            print(f"   Sending gesture {i+1}: {gesture}")
            await self.send_gesture(gesture)
            await asyncio.sleep(2)  # Wait for ACK
            
        # 5. Let it run for a while to receive IMU data
        print("\n4. Receiving IMU data for 10 seconds...")
        await asyncio.sleep(10)
        
        # 6. Disable notifications
        print("\n5. Disabling notifications...")
        await self.disable_notifications(IMU_CHAR_UUID)
        await self.disable_notifications(GESTURE_CHAR_UUID)
        
        print("\n Demo complete!")
    
    # Clean disconnect
    async def disconnect(self):
        if self.client and self.connected:
            await self.client.disconnect()
            print("Disconnected")

async def main():
    ble_client = ESP32BLEClient()
    
    try:
        # Connect to ESP32
        if not await ble_client.connect_to_esp32():
            return
            
        # Choose mode
        print("\nChoose mode:")
        print("1. Demo Sequence (automatic)")
        print("2. Interactive Mode (manual control)")
        
        choice = input("Enter 1 or 2: ").strip()
        
        if choice == "1":
            await ble_client.demo_sequence()
        else:
            await ble_client.interactive_mode()
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await ble_client.disconnect()

if __name__ == "__main__":
    # Run the client
    asyncio.run(main())