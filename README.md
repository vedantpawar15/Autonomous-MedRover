# Autonomous MedRover

Autonomous MedRover is a web-connected hospital delivery rover prototype.  
Users place medicine/supply delivery requests from a web portal, and an ESP32-based line-following robot navigates to the selected room route (A/B/C), performs delivery wait, and returns to base.

## Project Overview

- **Domain:** Indoor healthcare logistics automation
- **Robot:** ESP32 + IR line sensors + motor driver + DC motors
- **Control:** PD line following with junction-based routing and recovery logic
- **Cloud:** Supabase (order polling + status updates)
- **Frontend:** React/Vite web portal for order placement and tracking

## Repository Structure

- `hardware_esp32/` - ESP32 firmware sketches and calibration versions
  - `line_follow_v33.ino` - latest route logic (A/B/C + return behavior)
  - `line_follow_v32.ino` - previous stable iteration
- `web_portal/` - React app for ordering and order visibility
- `README.md` - this file

## How It Works

1. User places an order in the web portal (room code A/B/C).
2. ESP32 polls Supabase for `pending` orders.
3. Robot updates order to `in_transit` and starts mission.
4. Robot follows line, counts junctions, turns based on target room.
5. At room, robot pauses (`ROOM_WAIT_MS`), performs U-turn, and returns.
6. On home arrival, mission completes and order is marked `delivered`.

## Current Routing Logic (v33)

- **A:** turn left at junction 1, serve room, return to start.
- **B:** turn left at junction 2, serve room, return to start.
  - Return path is tuned to avoid unnecessary intermediate stop before home.
- **C:** continue on main line to top endpoint, serve room, U-turn, return to start.

> Route timings and turn thresholds depend on your floor track and must be tuned in firmware constants.

## Hardware Requirements

- ESP32 development board
- 5-channel IR line sensor array
- Motor driver (compatible with used pin mapping)
- 2 DC geared motors + wheels + chassis
- Battery/power module with common ground
- Black tape track on light floor

## Software Requirements

- Arduino IDE (or PlatformIO) with ESP32 board support
- Required libraries:
  - `WiFi.h`
  - `HTTPClient.h`
  - `ArduinoJson.h`
- Node.js + npm (for `web_portal`)

## Setup

### 1) Web Portal

```bash
cd web_portal
npm install
npm run dev
```

Create a local env file from `web_portal/.env.example`:

```bash
cd web_portal
cp .env.example .env
```

Then set:

- `VITE_SUPABASE_URL`
- `VITE_SUPABASE_ANON_KEY`

The web portal already reads these from `import.meta.env`.

### 1.1) Deploy Web Portal on Netlify

This repo includes root `netlify.toml` with:

- Base directory: `web_portal`
- Build command: `npm run build`
- Publish directory: `dist`
- SPA redirect rule to `index.html`

In Netlify:

1. Import the GitHub repository.
2. Build settings will auto-detect from `netlify.toml`.
3. Add environment variables in **Site settings -> Environment variables**:
   - `VITE_SUPABASE_URL`
   - `VITE_SUPABASE_ANON_KEY`
4. Trigger deploy.

### 2) Firmware

1. Open `hardware_esp32/line_follow_v33.ino`.
2. Set Wi-Fi and Supabase credentials.
3. Select ESP32 board and upload.
4. Open Serial Monitor at `115200`.

## Serial Commands (Firmware)

- `A` / `B` / `C` - choose route
- `g` - start mission
- `s` - stop
- `p` - print settings
- `i` - print IR snapshot
- `w` - IR watch mode
- `+` / `-` - base speed adjust
- `]` / `[` - Kp adjust
- `>` / `<` - Kd adjust

## Key Tuning Parameters (Firmware)

- `baseSpeed`, `turnSpeed`, `rightOffset`
- `JUNCTION_PAUSE_MS`, `ROOM_WAIT_MS`
- `BRANCH_MIN_TRAVEL_MS`, `LINE_END_CONFIRM_MS`
- `RETURN_MIN_TRAVEL_MS`, `RETURN_JUNCTION_COOLDOWN`

Tune these values on your real track for reliable turning and home return.

## Notes

- Keep secrets (Wi-Fi password, API keys) out of public repositories.
- Use calibration sketches in `hardware_esp32/` to validate sensor and motor setup before full mission runs.

## Future Improvements

- Dynamic map/routing instead of fixed A/B/C logic
- Obstacle detection and avoidance
- Better telemetry dashboard (battery, mission timestamps, errors)
- Safer credential handling and OTA updates
