# ESP32 Firmware & Calibration (`hardware_esp32/`)

This directory contains the Arduino sketches and firmware for the Autonomous MedRover ESP32 microcontroller.

---

## 📂 Key Files

- **`line_follow_v33_MainWorkingCode.ino`** — ⭐ **MAIN PRODUCTION FIRMWARE**
  - Complete line-following navigation with PD steering algorithm.
  - Automatic Supabase order polling over hospital Wi-Fi.
  - Room routing logic for Room A, Room B, and Room C with junction counting.
  - 220ms start kick pulse, watch mode suppression, branch travel nudge, and 180° return sequence.
  - Built-in interactive serial command interface (`115200` baud).

- **`calibration_v13.ino`** — Recommended sketch for isolated line-following and sensor threshold tuning.
- **`calibration_v1` through `calibration_v28`** — Historical iterative calibration and test sketches.
- **`line_follow_v30.ino` to `line_follow_v32.ino`** — Previous stable iterations kept for developmental reference.

---

## 📌 ESP32 Pin Mapping

| Component | Pin | ESP32 GPIO | Description |
| :--- | :--- | :---: | :--- |
| **L298N Motor Driver** | `ENA` | **14** | Left Motor PWM speed control |
| | `IN1` | **27** | Left Motor Direction A |
| | `IN2` | **26** | Left Motor Direction B |
| | `IN3` | **25** | Right Motor Direction A |
| | `IN4` | **33** | Right Motor Direction B |
| | `ENB` | **12** | Right Motor PWM speed control |
| **5-Channel IR Sensor** | `S1` | **34** | Far Left sensor (Junctions) |
| | `S2` | **35** | Mid Left sensor (PD Steering) |
| | `S3` | **32** | Center sensor (Track Lock) |
| | `S4` | **18** | Mid Right sensor (PD Steering) |
| | `S5` | **19** | Far Right sensor (Junctions) |

---

## ⌨️ Serial Commands (`115200` Baud)

| Command | Function |
| :---: | :--- |
| `A` / `B` / `C` | Select target room manually |
| `g` | Start mission |
| `s` | Stop motors immediately |
| `p` | Print current speeds, PID gains, and timing constants |
| `i` | Snapshot current IR sensor readings (`0` = white floor, `1` = black line) |
| `w` | Toggle continuous IR watch mode |
| `+` / `-` | Increase / decrease base forward speed |
| `]` / `[` | Increase / decrease Kp (Proportional gain) |
| `>` / `<` | Increase / decrease Kd (Derivative gain) |
| `)` / `(` | Increase / decrease turning speed |
| `n` / `m` | Adjust branch travel nudge time (+/- 50ms) |

---

## ⚙️ Uploading Instructions

1. Open `line_follow_v33_MainWorkingCode.ino` in Arduino IDE.
2. Ensure you have the `ArduinoJson` library installed.
3. Update `WIFI_SSID`, `WIFI_PASSWORD`, `SUPABASE_URL`, and `SUPABASE_ANON_KEY`.
4. Connect the ESP32 board, select **ESP32 Dev Module**, and click **Upload**.
5. Open Serial Monitor at **`115200`** baud to monitor connections and execution.
