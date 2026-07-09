# GNSS Anti-Spoofing & Jamming GPS Detection System

A real-time GNSS/GPS monitoring and defense project that detects **jamming**, **spoofing**, abnormal drift, satellite drops, and signal instability using a combination of embedded firmware, machine-learning models, and a live dashboard.

![Hardware setup](attachment:image.png)

![AirWatch Sentinel dashboard](attachment:image.png)

## Overview

This project combines:

- **ESP32-based GNSS acquisition**
- **Live telemetry dashboard**
- **AI/ML-based anomaly detection**
- **Attack simulation controls**
- **Data logging and model training scripts**
- **A polished monitoring UI** showing satellite count, HDOP, drift, trust score, and risk indicators

The dashboard screenshot in this project shows the system detecting **jamming**, tracking **signal metrics**, and displaying a **position and gate** panel along with an **anti-spoofing intelligence** section.

## Key Features

- Real-time GNSS status monitoring
- Jamming and spoofing detection logic
- Stability gate and safe-mode style decisioning
- Satellite count, HDOP, drift, speed, and anomaly tracking
- Live map view with attack heatmap support
- Attack simulation buttons for testing defense logic
- Data logging for normal and jammed GPS traces
- ML model training pipeline for classification

## Hardware Used

The setup shown in the photos includes:

- **ESP32 development board**
- **GNSS/GPS module**
- **Breadboard wiring setup**
- **Power input / USB connection**
- Supporting jumper wires and connectors

## Photos

### Hardware Prototype

The first photo shows the physical prototype on a breadboard with the GNSS module connected to the controller board.

### Dashboard Screenshot

The dashboard screenshot shows the AirWatch Sentinel interface with:

- System status
- Signal metrics
- Simulation controls
- Position and gate information
- Anti-spoofing intelligence
- Map visualization

## Project Structure

Important files included in this repository:

- `GNSS.c` - Arduino/ESP32 firmware export for the GNSS security terminal
- `gnss_model.h` - embedded ML model header
- `gnss_model (2).h` - alternate model header
- `app.py` - Streamlit-based live GNSS monitor
- `gps_logger.py` - serial logger for normal GPS data
- `jam_logger.py` - serial logger for jammed GPS data
- `train_model.py` - ML training script using XGBoost
- `analyze_data.ipynb` - notebook for analysis and experimentation
- `normal_gps.csv` - normal GPS dataset
- `jam_gps.csv` - jammed GPS dataset
- `advanced_gnss_model.pkl` - trained model artifact
- `gnss_detection_model.pkl` - detection model artifact
- `attack_clusters_tree.json` - clustered attack scenario data
- `cluster_visualization.html` - attack clustering visualization
- `folder_structure.log` - generated folder tree for attack scenarios

## How It Works

1. The GNSS receiver feeds raw location data into the ESP32.
2. The firmware computes features such as:
   - satellite count
   - HDOP
   - drift
   - speed jump
   - satellite drop rate
   - anomaly score
3. The model and rule logic evaluate whether the signal looks normal or compromised.
4. The web dashboard displays the current security state in real time.
5. Logged data can be used to retrain or improve the classifier.

## Software Components

### Firmware / Embedded Side

The firmware implements the GNSS monitoring terminal and the web dashboard interface. It uses Wi-Fi, GPS parsing, and a model-based decision layer.

### Python Side

The Python scripts provide:

- live monitoring via Streamlit
- serial logging
- model training
- analysis support

## ML / Data Pipeline

The training script builds features from GPS traces and labels them as:

- `0` = normal
- `1` = jammed

It then trains an **XGBoost classifier** and saves the final model as `advanced_gnss_model.pkl`.

## Setup / Usage

### 1. Flash the ESP32 firmware

Use the contents of `GNSS.c` / the Arduino sketch logic for the ESP32 GNSS terminal.

### 2. Run the dashboard

```bash
streamlit run app.py
```

### 3. Log GPS data

```bash
python gps_logger.py
python jam_logger.py
```

### 4. Train the model

```bash
python train_model.py
```

## Notes

- Update Wi-Fi credentials and serial port settings before deployment.
- The current Python logger scripts use `COM6`, which may need to be changed for your system.
- The dashboard expects the ESP32 to expose a `/status` endpoint.

## Drive Folder

Project files and supporting media are also available here:

[Google Drive Folder](https://drive.google.com/drive/folders/1NiBHpu8zh0zpMV2djbW56g29uM4IjQa8?usp=sharing)

## Repository Link

GitHub repository:

https://github.com/Yogesh-77/GNSS_ANTI_SPOOFING_JAMMING_GPS/blob/

## Suggested GitHub Description

> Real-time GNSS anti-spoofing and jamming detection system using ESP32, machine learning, and a live monitoring dashboard.

## License

Add your preferred license here before publishing.
