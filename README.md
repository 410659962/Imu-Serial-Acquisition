# IMU Array Data Acquisition Tool

[![Language](https://img.shields.io/badge/Language-C++/QT-green?logo=github&logoColor=white)](https://github.com/410659962/imu-serial-acquisition)
[![Platform](https://img.shields.io/badge/Platform-Windows-green?logo=github&logoColor=white)](https://github.com/410659962/imu-serial-acquisition)
A professional Qt-based serial port host computer application for real-time acquisition, visualization, and storage of multi-IMU sensor array data.

## 📌 Overview

This application is designed to receive and process data from a 9-IMU sensor array (each IMU providing 3-axis acceleration and 3-axis gyroscope data) via serial port. It features real-time data visualization, statistical analysis, CSV data logging, and automatic port detection.

## ✨ Key Features

**Multi-IMU Support:** Simultaneously process data from 9 IMU sensors
**Real-time Visualization:**

- Interactive chart displaying mean acceleration (g) and gyroscope (dps) values
- 10-second rolling window with smooth scrolling
- Color-coded axes (acceleration: solid lines, gyroscope: dashed lines)
  **Data Logging:**
- Automatic CSV file generation on desktop (IMU_Data_YYYYMMDD_hhmm.csv)
- Millisecond-precision timestamps (UTC)
- Float values (6 decimal places)
- Optional auto-stop timer for timed recordings
  **Serial Communication:**
- Automatic serial port scanning (every 2 seconds)
- Frame synchronization with header/tail detection
- Buffer overflow protection and error recovery
  **Performance Optimized:**
- UI update decoupled from data reception (10 FPS display refresh)
- Chart update throttling to prevent UI lag
- Efficient memory management for continuous operation

## 🔌 Data Frame Format

The application expects a strict binary protocol:
|Component|Size|Value/Description|
|----|----|----|
|Header|2 bytes|0xAA 0x55|
|Payload|216 bytes|9 IMUs × 6 floats (3 accel + 3 gyro) × 4 bytes|
|Tail|4 bytes|0x00 0x00 0x80 0x7F|
|Total Frame|222 bytes||
Each float value follows IEEE 754 little-endian format

## 🚀 Quick Start

**Prerequisites**

- Qt 5.12.0
- QSerialPort module

**Building from Source**

```
# Clone repository
git clone https://github.com/410659962/imu-serial-acquisition.git
cd IMU-Array-Acquisition

# Using Qt Creator (recommended)
# 1. Open IMU_Acquisition.pro
# 2. Configure kit (Desktop Qt ...)
# 3. Build → Run
```

**Usage**
1、Connect your IMU array device via USB/serial adapter
2、Launch the application
3、Select detected COM port from dropdown menu
4、Click "打开串口" (Open Serial Port) button
5、Verify data reception in the display panels
6、Click "开始保存" (Start Saving) to record data to CSV
7、Use "清除接收" (Clear) to reset displays and charts

## 💾 Data Output Format (CSV)

Saved files contain timestamped data in the following format:

```
Timestamp,IMU1_Ax,IMU1_Ay,IMU1_Az,IMU1_Gx,IMU1_Gy,IMU1_Gz,...,IMU9_Ax,...,IMU9_Gz
1707234567890,0.012345,-0.023456,0.987654,1.234567,-2.345678,3.456789,...,...
```

- **Timestamp:** Milliseconds since Unix epoch (UTC)
- **Ax/Ay/Az:** Acceleration in g (gravity units)
- **Gx/Gy/Gz:** Gyroscope in degrees per second (dps)
- Files saved to user's desktop by default

## ⚙️ Configuration Parameters

| Parameter           | Value     | Description                   |
| ------------------- | --------- | ----------------------------- |
| IMU_COUNT           | 9         | Number of IMU sensors         |
| FRAME_SIZE          | 222 bytes | Total frame size              |
| DATA_INTERVAL_MS    | 100 ms    | UI update interval            |
| MAX_DISPLAY_SECONDS | 10 s      | Chart history window          |
| Chart_FPS           | 10 Hz     | Chart refresh rate            |
| Default Baud Rate   | 460800    | Optimized for high-speed data |

## 📊 Performance Metrics

- Theoretical Data Rate: 100 Hz (22.2 KB/s at 460800 baud)
- Actual Frequency: Dynamically calculated and displayed
- Memory Usage: Constant (rolling buffer with fixed history)

## 🛠️ Customization

To adapt for different sensor configurations:

- Modify constants in mainwindow.h:

```
static const int IMU_COUNT = 9;           // Change sensor count
static const int DATA_PER_IMU = 6;        // Change data channels per IMU
```

- Adjust chart axis ranges in initCharts():

```
axisYAccel->setRange(-2, 2);      // Acceleration range (g)
axisYGyro->setRange(-250, 250);   // Gyroscope range (dps)
```

## 🙏 Acknowledgements

- Qt Framework - Cross-platform application development
- Qt SerialPort Module - Serial communication abstraction
- Qt Charts Module - Data visualization components
- Inspired by industrial IMU array monitoring requirements

## 📬 Support & Contributing

- In addition to supporting multithreading, please advise whether any other performance optimization strategies are available.
