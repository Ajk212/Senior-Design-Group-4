import serial
import csv
import datetime
import os

#Serial config
SERIAL_PORT = 'COM4'
BAUD_RATE = 115200
SENSOR_FRAME_LENGTH = 17 

#Folder to store gesture data (.csv files)
DATA_DIR = "gesture_datasets"
os.makedirs(DATA_DIR, exist_ok=True)

#name of file
gesture_label = "mouse_right" 
ready = input("Enter to continue: ").strip()

#Create unique filename
timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
filename = os.path.join(DATA_DIR, f"{gesture_label}_{timestamp}.csv")

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
print(f"Recieving data through {SERIAL_PORT}")

#Open file to write data
with open(filename, 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    
    #Headers of csv
    header = [
        "accel1_x", "accel1_y", "accel1_z",
        "gyro1_x", "gyro1_y", "gyro1_z",
        "accel2_x", "accel2_y", "accel2_z",
        "gyro2_x", "gyro2_y", "gyro2_z",
        "flex1", "flex2", "flex3", "flex4", "flex5",
        "gesture_label"
    ]
    writer.writerow(header)

    try:
        while True:
            line = ser.readline().decode('utf-8').strip()
            if not line:
                continue
            
            try:
                values = [float(x) for x in line.split(',')]
            except ValueError:
                continue  #skip non-sensor lines
            
            #Yse only valud frames
            if len(values) == SENSOR_FRAME_LENGTH:
                values.append(gesture_label)
                writer.writerow(values)
                print(values)
    
    #Stop data collection
    except KeyboardInterrupt:
        print("\nRecording stopped.")
        print(f"Data saved to {filename}")
        ser.close()