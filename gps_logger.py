import serial

ser = serial.Serial('COM6',115200)

file = open("gps_data.csv","w")

print("Logging Started...")

while True:
    line = ser.readline().decode('utf-8',errors='ignore').strip()

    if line:
        print(line)
        file.write(line + "\n")
        file.flush()