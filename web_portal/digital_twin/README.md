# MedRover 3D Digital Twin & Telemetry Simulator 🏥🤖

An interactive 3D digital twin dashboard for the Autonomous MedRover system, built using **React 19**, **Three.js**, and **React Three Fiber (`@react-three/fiber`, `@react-three/drei`)**.

---

## 🌟 Features

- **3D Hospital Corridor Simulation**: Realistic hospital floor layout with docking bay, junction nodes (J1, J2, J3), rooms A/B/C, and luminous guidance tracks.
- **Dynamic Rover Mesh**: Rover model with rotating wheels, headlights, and an animated scanning LIDAR beam.
- **Live Supabase Realtime Telemetry**:
  - Subscribes to changes on the `rover_status` and `orders` tables.
  - Automatically mirrors real-world rover missions and kinematics in 3D.
- **Multi-Camera Director**:
  - **Free Orbit**: OrbitControls for panning, zooming, and inspecting from any angle.
  - **Top-Down**: 2D overhead map perspective for hospital floor overview.
  - **Follow Rover**: 3rd-person chase camera locked to the rover during navigation.
- **Interactive Telemetry HUD**:
  - Live indicators for Battery %, Wi-Fi RSSI (dBm), (X, Z) coordinates, yaw angle, speed, and mission progress.
  - Manual mission dispatch buttons (Room A, Room B, Room C).
  - Emergency Stop and Return-to-Base controls.
  - Standalone simulation toggle for testing without active hardware.

---

## 🚀 Quick Start

```bash
# Install dependencies
npm install

# Configure environment
cp ../web_portal/.env.example .env
```

Ensure `.env` contains:
```env
VITE_SUPABASE_URL=https://your-project.supabase.co
VITE_SUPABASE_ANON_KEY=your-supabase-anon-key
```

Run development server:
```bash
npm run dev -- --port 5174
```

Access the 3D twin in your browser at `http://localhost:5174`.
