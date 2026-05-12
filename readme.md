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
- **WiFi Manager with password-protected config portal**  
- **Over-the-air WiFi reconfiguration**  

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
- GitHub Actions (CI/CD deployment)  

---

## Installation

### Hardware Setup

1. Connect BME680 via I2C (SDA→GPIO21, SCL→GPIO22)
2. Connect magnetic reed switch to **GPIO4** (RTC GPIO — required for deep sleep wake)
3. Power ESP32 using USB, adapter, or battery (Li-ion 18650 + TP4056)
4. Mount sensors inside toilet enclosure safely

### Software Setup

1. Upload firmware to ESP32 via Arduino IDE  
2. On first boot, ESP32 creates a Wi-Fi access point named **`AirLoo-Config`**  
3. Connect to that AP and open **`http://192.168.4.1`** in your browser  
4. Select your Wi-Fi network, enter credentials, and set an **admin password**  
5. ESP32 reboots and connects to your network automatically  

---

## WiFi Manager — Configuration Portal

The ESP32 includes a built-in WiFi Manager that eliminates the need to hardcode Wi-Fi credentials.

### First-Time Setup

1. Power on the ESP32  
2. Connect your phone/laptop to the **`AirLoo-Config`** Wi-Fi AP  
3. Open a browser to **`http://192.168.4.1`**  
4. Select your Wi-Fi network from the scanned list  
5. Enter your Wi-Fi password  
6. **Create an admin password** (min 4 characters) to protect the config portal  
7. Click "Connect" — the device saves credentials and reboots  

### Reconfiguring Wi-Fi Later

- **Hold the BOOT button (GPIO0) for 5 seconds** during normal operation  
- The device restarts into config portal mode  
- **Enter your admin password** when prompted, then change Wi-Fi settings  

### Factory Reset

- **Hold the BOOT button while powering on** the device  
- This clears all saved data (Wi-Fi credentials + admin password)  
- Device boots into first-time setup mode  

### Security

| Threat | Protection |
|--------|-----------|
| Someone holds BOOT button | Config portal starts but **admin password is required** to make changes |
| Random person connects to `AirLoo-Config` | Sees a login page — can't proceed without the password |
| Physical tampering | Full factory reset requires holding BOOT **at the moment of power-on** |

---

## Configuration

### ESP32 Firmware (`secrets.h`)

The ESP32 firmware only needs the Firebase API key in `Firebase/secrets.h`.  
Wi-Fi credentials are configured through the web portal (not stored in code).

Use `Firebase/secrets.h.example` as a template:

```cpp
#define API_KEY "your_firebase_api_key"
```

Rename the example file and fill in your values:

```bash
cp Firebase/secrets.h.example Firebase/secrets.h
```

This file is gitignored to prevent accidental credential leaks.

### Firebase Project ID

The Firebase project ID is configured directly in `Firebase/Firebase.ino`:

```cpp
String projectId = "your-firebase-project-id";
```

### Dashboard

The monitoring dashboard (`dashboard/dashboard.html`) is a static frontend hosted on **GitHub Pages**.  

Firebase credentials are injected at deploy time via **GitHub Actions** (never committed to the repo).  

Set the following **repository secrets** in `Settings → Secrets and variables → Actions`:

| Secret | Value |
|--------|-------|
| `FIREBASE_PROJECT_ID` | Your Firebase project ID |
| `FIREBASE_API_KEY` | Your Firebase API key |
| `FIRESTORE_COLLECTION` | Collection name (e.g. `events`) |

Every push to `main` triggers a deployment that injects these secrets into `dashboard/env.js` and publishes to GitHub Pages.

---

## Button Controls Summary

| Action | GPIO | Behavior |
|--------|------|----------|
| Hold BOOT 5s (operation) | GPIO0 | Restarts config portal (password required) |
| Hold BOOT on power-up | GPIO0 | Factory reset (clears all saved data) |

---

## Battery Operation

The firmware is optimized for deep-sleep battery operation using `esp_sleep_enable_ext0_wakeup` on GPIO4 (reed switch).

### Power Architecture

- ESP32 spends **99%+ of time in deep sleep** (~10 µA)
- Reed switch state change (GPIO4, RTC GPIO) wakes the ESP32
- ESP32 boots, connects WiFi, sends event to Firestore, shows OLED for 3s, then goes back to sleep
- Wake level alternates between HIGH/LOW to detect both OPEN and CLOSE events
- OLED is powered off via command (`SSD1306_DISPLAYOFF`) before sleep
- WiFi is fully disconnected and radio turned off before sleep
- CPU runs at 80 MHz to reduce active power draw
- Modem sleep enabled during WiFi idle (`WiFi.setSleep(true)`)

### Estimated Battery Life

| Events/day | Active time | Avg current | Battery life (2000 mAh) |
|-----------|-------------|-------------|------------------------|
| 50 | ~4 min | ~0.8 mA | **~100 days** |
| 100 | ~8 min | ~1.6 mA | **~50 days** |
| 200 | ~17 min | ~3.2 mA | **~25 days** |

Assumes ~3s WiFi connect + ~3s OLED display per event. Actual life depends on WiFi signal strength and battery chemistry (Li-ion recommended).

### Cold Boot (Power On)

On a cold boot (not from deep sleep), the firmware:
1. Shows the current door state on OLED
2. Goes to deep sleep **without sending a false event**
3. Normal door events trigger wake + send as usual

### Battery Tips

- Use a **Li-ion 18650** (2000–3000 mAh) with a TP4056 charger module
- Add a **P-channel MOSFET** to cut OLED power completely during sleep (the software OFF command still has ~1 µA leakage)
- Keep WiFi signal strong — weak signal increases connection time and power draw
- Disable the OLED in `secrets.h` if not needed (comment out the display code)
- For extreme battery life, consider batching events and sending every N wakes instead of each event

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
