# Smart Public Toilet Monitoring System

## Overview

The Smart Public Toilet Monitoring System is an IoT-enabled sanitation management solution designed to improve hygiene standards, maintenance efficiency, and user experience in public restroom facilities.

The system uses a **magnetic door switch** to monitor usage frequency and a **BME680 environmental sensor** to measure air quality, temperature, humidity, and odor-related gas conditions. Real-time data is transmitted to a monitoring dashboard where maintenance teams can receive alerts and respond promptly.

This solution supports condition-based cleaning rather than fixed maintenance schedules, leading to cleaner facilities and better resource utilization.

---

## Problem Statement

Public toilets often suffer from:

- Poor hygiene standards  
- Delayed cleaning response  
- Unpleasant odor  
- Overuse without maintenance  
- Lack of usage analytics  
- Inefficient manual inspection systems  

Traditional cleaning schedules are usually time-based rather than need-based, resulting in poor operational efficiency and unsatisfactory user experience.

---

## Proposed Solution

This project introduces a smart monitoring system capable of:

- Detecting toilet usage through door activity  
- Monitoring environmental conditions continuously  
- Identifying odor buildup and poor air quality  
- Sending maintenance alerts automatically  
- Recording historical usage data  
- Supporting data-driven cleaning schedules  

---

## System Components

| Component | Function |
|----------|----------|
| ESP32 | Main controller with Wi-Fi |
| Magnetic Door Switch | Detects door open/close cycles |
| BME680 Sensor | Measures air quality, gas, humidity, temperature |
| Power Supply | Powers the device |
| OLED Display (Optional) | Displays local status |

---

## Working Principle

### Usage Detection

A magnetic switch is mounted on the toilet door. Each door open-close cycle is treated as a usage event.

### Environmental Monitoring

The BME680 continuously measures:

- Temperature  
- Humidity  
- Atmospheric Pressure  
- Gas Resistance (used as odor / air quality indicator)

### Alert Mechanism

When:

- Usage exceeds threshold  
- Odor level increases significantly  
- Humidity remains abnormal  

The system generates a maintenance alert.

### Cloud Dashboard

Sensor data is uploaded through Wi-Fi to a dashboard for monitoring and analytics.

---

## Features

- Real-time monitoring  
- Door usage counter  
- Odor detection  
- Air quality monitoring  
- Remote dashboard access  
- Automated cleaning alerts  
- Historical data logging  
- Scalable multi-location deployment  

---

## Applications

- Municipal public toilets  
- Railway stations  
- Bus terminals  
- Airports  
- Shopping malls  
- Educational institutions  
- Hospitals  
- Smart city sanitation systems  

---

## Benefits

- Improved hygiene standards  
- Faster maintenance response  
- Reduced manual inspections  
- Better manpower allocation  
- Increased user satisfaction  
- Data-driven operations  

---

## Technology Stack

### Hardware
- ESP32  
- BME680 Sensor  
- Magnetic Reed Switch  

### Software
- Arduino IDE  
- HTML / CSS / JavaScript Dashboard  
- Firebase / Cloud Database  
- Wi-Fi Connectivity  

---

## Installation

### Hardware Setup

1. Connect BME680 via I2C  
2. Connect magnetic switch to digital GPIO pin  
3. Power ESP32 using USB or adapter  
4. Mount sensors inside toilet enclosure safely

### Software Setup

1. Upload firmware to ESP32  
2. Configure Wi-Fi credentials  
3. Configure cloud database credentials  
4. Open monitoring dashboard  

---

## Configuration

### Secrets (`secrets.h`)

The ESP32 firmware requires a `Firebase/secrets.h` file for Wi-Fi and Firebase credentials.  
Use `Firebase/secrets.h.example` as a template:

```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASS "your_wifi_password"
#define API_KEY "your_firebase_api_key"
```

Rename the example file and fill in your values:

```bash
cp Firebase/secrets.h.example Firebase/secrets.h
```

This file is already gitignored to prevent accidental credential leaks.

### Firebase Project ID

The Firebase project ID is configured directly in `Firebase/Firebase.ino`:

```cpp
String projectId = "your-firebase-project-id";
```

### Dashboard

The monitoring dashboard (`dashboard/dashboard.html`) is a static frontend.  
Firebase credentials are configured via `dashboard/env.js` (gitignored).  

Use the example template to set up your config:

```bash
cp dashboard/env.example.js dashboard/env.js
```

Then edit `dashboard/env.js` with your Firebase project details:

```js
const ENV = {
  FIREBASE_PROJECT_ID: 'your_firebase_project_id',
  FIREBASE_API_KEY: 'your_firebase_api_key',
  FIRESTORE_COLLECTION: 'events',
};
```

---

## Future Enhancements

- Water level monitoring  
- Soap / tissue stock detection  
- QR code feedback system  
- Predictive maintenance using AI  
- GSM / SMS alerts  
- Integration with city control rooms  

---

## Conclusion

The Smart Public Toilet Monitoring System offers a practical and scalable solution for improving sanitation infrastructure. By combining occupancy monitoring and environmental sensing, the system enables proactive maintenance, cleaner facilities, and more efficient public service delivery.

---

## License

This project is intended for educational, research, and smart city development purposes.