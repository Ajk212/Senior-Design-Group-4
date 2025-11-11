import asyncio
import struct
from bleak import BleakScanner, BleakClient

# UUIDs
SERVICE_UUID = "000000ff-0000-1000-8000-00805f9b34fb"  # 0x00FF 
TX_UUID      = "0000ff01-0000-1000-8000-00805f9b34fb"  # 0xFF01 (read/notify)
RX_UUID      = "0000ff02-0000-1000-8000-00805f9b34fb"  # 0xFF02 (write)
CCCD_UUID     = "00002902-0000-1000-8000-00805f9b34fb"  # CCCD

IMU_FRAME_FORMAT = "<14fLLb"   # expected 65 bytes

class IMUFrame:
    def __init__(self, data: bytes):
        if len(data) != 65:
            raise struct.error(f"Expected 65 bytes, got {len(data)}")
        unpacked = struct.unpack(IMU_FRAME_FORMAT, data)
        self.accel1_x, self.accel1_y, self.accel1_z = unpacked[0:3]
        self.gyro1_x,  self.gyro1_y,  self.gyro1_z  = unpacked[3:6]
        self.temp1 = unpacked[6]
        self.accel2_x, self.accel2_y, self.accel2_z = unpacked[7:10]
        self.gyro2_x,  self.gyro2_y,  self.gyro2_z  = unpacked[10:13]
        self.temp2 = unpacked[13]
        self.timestamp   = unpacked[14]
        self.frame_count = unpacked[15]
        self.status      = unpacked[16]

def pretty_print_imu(frame: IMUFrame):
    print(f"\n IMU Frame #{frame.frame_count}:")
    print(f"   IMU1 - Accel: ({frame.accel1_x:6.2f}, {frame.accel1_y:6.2f}, {frame.accel1_z:6.2f}) m/s²")
    print(f"          Gyro:  ({frame.gyro1_x:6.2f}, {frame.gyro1_y:6.2f}, {frame.gyro1_z:6.2f}) °/s")
    print(f"          Temp:  {frame.temp1:5.1f}°F")
    print(f"   IMU2 - Accel: ({frame.accel2_x:6.2f}, {frame.accel2_y:6.2f}, {frame.accel2_z:6.2f}) m/s²")
    print(f"          Gyro:  ({frame.gyro2_x:6.2f}, {frame.gyro2_y:6.2f}, {frame.gyro2_z:6.2f}) °/s")
    print(f"          Temp:  {frame.temp2:5.1f}°F")
    print(f"   Metadata - Timestamp: {frame.timestamp}  Status: {frame.status}")

class ESP32BLEClient:
    def __init__(self, name_substring="ESP32C6-HANDSENSE"):
        self.name_substring = name_substring
        self.client = None
        self.connected = False
        self._notify_on = False
        self.services = None

    def _on_disconnect(self, _client):
        print("Disconnected from ESP32")
        self.connected = False
        self._notify_on = False

    async def _fetch_services(self):
        if hasattr(self.client, "get_services"):
            return await self.client.get_services()   # Bleak ≥ 0.22
        return self.client.services                  # Older Bleak

    async def connect(self) -> bool:
        print("Scanning for ESP32...")
        devices = await BleakScanner.discover()
        target = next((d for d in devices if d.name and self.name_substring in d.name), None)
        if not target:
            print("ESP32 not found!")
            return False

        self.client = BleakClient(target.address, disconnected_callback=self._on_disconnect)
        await self.client.connect()
        self.connected = True
        print(f"Connected to {target.name}")

        self.services = await self._fetch_services()
        await self._validate_services()
        return True

    async def _validate_services(self):
        svcs = self.services
        print("\n Services Discovered:")
        for s in svcs:
            print(f"Service: {s.uuid}")
            for c in s.characteristics:
                print(f"  Characteristic: {c.uuid}")
                print(f"  Properties: {c.properties}")

        svc = svcs.get_service(SERVICE_UUID)
        if svc is None:
            raise RuntimeError("Expected custom service FF00 not found")

        tx = svcs.get_characteristic(TX_UUID)
        if tx is None or ("notify" not in tx.properties and "indicate" not in tx.properties):
            raise RuntimeError("TX characteristic FF01 not found or missing notify/indicate")

        rx = svcs.get_characteristic(RX_UUID)
        if rx is None or ("write" not in rx.properties and "write-without-response" not in rx.properties):
            raise RuntimeError("RX characteristic FF02 not found or missing write property")

    # ---- Basic ops

    async def read_tx(self):
        if not self.connected or self.client is None:
            return
        try:
            data = await self.client.read_gatt_char(TX_UUID)
            print(f"TX ({TX_UUID}) read: {data.hex()}")
            return data
        except Exception as e:
            print(f"Read TX failed: {e}")

    async def write_rx(self, data: bytes | str, response: bool = True):
        if not self.connected or self.client is None:
            return False
        if isinstance(data, str):
            data = data.encode("utf-8")
        try:
            await self.client.write_gatt_char(RX_UUID, data, response=response)
            print(f"Sent to RX ({RX_UUID}): {data.hex()}")
            return True
        except Exception as e:
            print(f"Write to RX failed: {e}")
            return False

    # ---- Notifications

    def _notification_cb(self, sender: str, data: bytes):
        # Try IMU frame first; if not 65 bytes, print as text/hex
        try:
            frame = IMUFrame(data)
            pretty_print_imu(frame)
        except Exception:
            # Fallback: dump raw
            print(f"Notify from {sender}: {data.hex()}")
            try:
                txt = data.decode("utf-8", errors="ignore")
                if txt.strip():
                    print(f" As text: {txt}")
            except Exception:
                pass

    async def enable_notifications(self):
        if not self.connected or self.client is None:
            return False
        if self._notify_on:
            return True
        try:
            # Let Bleak handle CCCD write for TX (FF01)
            await self.client.start_notify(TX_UUID, self._notification_cb)
            self._notify_on = True
            print(f"Enabled notifications for TX ({TX_UUID})")
            return True
        except Exception as e:
            print(f"Enable notifications failed: {e}")
            return False

    async def disable_notifications(self):
        if not self.connected or self.client is None:
            return
        if not self._notify_on:
            return
        try:
            await self.client.stop_notify(TX_UUID)
            self._notify_on = False
            print(f"Disabled notifications for TX ({TX_UUID})")
        except Exception as e:
            print(f"Disable notifications failed: {e}")

    async def interactive(self):
        if not self.connected:
            return
        print("\nInteractive Mode:")
        print("  read                -> read TX")
        print("  write <text/hex>    -> write RX (prefix 0x for hex bytes)")
        print("  notify on|off       -> enable/disable notifications on TX")
        print("  quit                -> exit")
        while self.connected:
            try:
                cmd = input(">>> ").strip()
            except (EOFError, KeyboardInterrupt):
                break
            if not cmd:
                continue
            if cmd == "quit":
                break
            if cmd == "read":
                await self.read_tx()
                continue
            if cmd.startswith("notify"):
                _, *rest = cmd.split()
                if rest and rest[0].lower() == "on":
                    await self.enable_notifications()
                elif rest and rest[0].lower() == "off":
                    await self.disable_notifications()
                else:
                    print("usage: notify on|off")
                continue
            if cmd.startswith("write "):
                payload = cmd[6:].strip()
                # Accept hex payloads like: 0xAABBCCDD
                if payload.startswith("0x"):
                    try:
                        b = bytes.fromhex(payload[2:])
                    except ValueError as e:
                        print(f"bad hex: {e}")
                        continue
                    await self.write_rx(b, response=True)
                else:
                    await self.write_rx(payload, response=True)
                continue
            print("unknown command")

    async def disconnect(self):
        if self.client and self.connected:
            await self.client.disconnect()
            print("Disconnected")

async def main():
    cli = ESP32BLEClient()
    try:
        if not await cli.connect():
            return
        await cli.interactive()
    finally:
        await cli.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
