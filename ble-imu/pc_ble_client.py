import struct
import asyncio
import time
from bleak import BleakScanner, BleakClient

# ESP32 UUIDs for reference 
SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"  # Service 0x00FF
CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"     # Characteristic 0xFF01
CCC_DESCRIPTOR_UUID = "00002902-0000-1000-8000-00805f9b34fb"  # CCC Descriptor

IMU_FRAME_FORMAT = '<14fLLb' # Little-endian, 14 floats (f), 2 unsigned long (L), 1 char (b) - Total 17 fields
# IMU_FRAME_FORMAT = '<10fLLb'  # Little-endian, 10 floats, 2 unsigned long, 1 char - Total 13 fields
# IMU_FRAME_SIZE = 10*4 + 2*4 + 1  # 10 floats * 4 bytes + 2 uint32 * 4 bytes + 1 uint8 = 49 bytes
IMU_FRAME_SIZE = 14*4 + 2*4 + 1  # 14 floats * 4 bytes + 2 uint32 * 4 bytes + 1 uint8 = 65 bytes


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

def imu_notification_handler(sender, data):
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

class ESP32BLEClient:
    def __init__(self):
        self.client = None
        self.connected = False
        
    # Handle incoming notifications from ESP32
    def notification_handler(self, sender, data):
        print(f"Received notification from {sender}: {data.hex()}")
        
        # Convert to readable text if possible
        try:
            text = data.decode('utf-8', errors='ignore')
            print(f"As text: {text}")
        except:
            pass
            
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
            print(f"Service: {service.uuid}")
            for char in service.characteristics:
                print(f"  Characteristic: {char.uuid}")
                print(f"  Properties: {char.properties}")
                
    # Read from characteristic
    async def read_characteristic(self):
        if not self.connected:
            return
            
        print("\n Reading Characteristic:")
        
        # Read from Service
        try:
            value = await self.client.read_gatt_char(CHAR_UUID)
            print(f"Characteristic ({CHAR_UUID}): {value.hex()}")
        except Exception as e:
            print(f"  Failed to read characteristic: {e}")
            
    # Write data to characteristic
    async def write_to_characteristic(self, data):
        if not self.connected:
            return False
            
        try:
            # Convert string to bytes if needed
            if isinstance(data, str):
                data = data.encode('utf-8')
                
            await self.client.write_gatt_char(CHAR_UUID, data, response=True)
            print(f"Sent to {CHAR_UUID}: {data.hex()}")
            return True
        except Exception as e:
            print(f"Write failed: {e}")
            return False
            
    # Enable notifications for characteristic
    async def enable_notifications(self):
        if not self.connected:
            return False
            
        try:
            # Start notifications
            await self.client.start_notify(CHAR_UUID, imu_notification_handler)
            print(f"Enabled notifications for {CHAR_UUID}")
            return True
        except Exception as e:
            print(f"Failed to enable notifications: {e}")
            return False
            
    # Disable notifications
    async def disable_notifications(self):
        if not self.connected:
            return
            
        try:
            await self.client.stop_notify(CHAR_UUID)
            print(f"Disabled notifications for {CHAR_UUID}")
        except Exception as e:
            print(f"Failed to disable notifications: {e}")
            

    # Interactive mode for sending/receiving data
    async def interactive_mode(self):
        if not self.connected:
            return
            
        print("\n Interactive Mode - Type commands:")
        print("  'read' - Read characteristic")
        print("  'write <data>' - Write data to characteristic")
        print("  'notify <on/off>' - Enable/disable notifications")  
        print("  'quit' - Exit")
        
        while self.connected:
            try:
                command = input("\n>>> ").strip().lower()
                
                if command == 'quit':
                    break
                elif command == 'read':
                    await self.read_characteristic()
                elif command.startswith('write'):
                    parts = command.split()
                    if len(parts) >= 2:
                        data_text = ' '.join(parts[1:])
                        await self.write_to_characteristic(data_text)
                    else:
                        print("Usage: write <data>")
                        
                elif command.startswith('notify'):
                    parts = command.split()
                    if len(parts) >= 2:
                        action = parts[1].lower()
                        
                        if action == 'on':
                            await self.enable_notifications()
                        elif action == 'off':
                            await self.disable_notifications()
                        else:
                            print("Use 'on' or 'off'")
                    else:
                        print("Usage: notify <on/off>")
                else:
                    print("Unknown command")
                    
            except Exception as e:
                print(f"Error: {e}")

    # Run a demo sequence automatically
    async def demo_sequence(self):
        if not self.connected:
            return
            
        print("\n Starting Demo Sequence...")
        
        # 1. Read initial values
        await self.read_characteristic()
        
        # 2. Enable notifications
        await self.enable_notifications()
        
        # 3. Send some test data
        test_messages = [
            b"Hello ESP32!",
            b"Test 123",
            bytes([0xAA, 0xBB, 0xCC, 0xDD]),
            b"Python to ESP32"
        ]
        
        for i, message in enumerate(test_messages):
            print(f"\n Sending message {i+1}...")
            await self.write_to_characteristic(message)
            await asyncio.sleep(1)  # Wait for response
            
        # 4. Disable notifications
        await self.disable_notifications()
        
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
            
        # Explore services
        await ble_client.explore_services()
        
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