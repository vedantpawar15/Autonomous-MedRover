# Autonomous MedRover 🚑🤖

> An end-to-end, IoT-powered autonomous hospital logistics rover featuring an **ESP32 line-following robot**, an **offline-resilient React web portal**, a **3D Three.js Digital Twin telemetry simulator**, and **Supabase Realtime cloud synchronization**.

---

## 📑 Table of Contents

- [System Overview](#-system-overview)
- [Architecture & Dataflow](#-architecture--dataflow)
- [Key Features by Subsystem](#-key-features-by-subsystem)
  - [1. Web Portal (`web_portal/`)](#1-web-portal-web_portal)
  - [2. 3D Digital Twin Simulator (`digital_twin/`)](#2-3d-digital-twin-simulator-digital_twin)
  - [3. ESP32 Autonomous Hardware Rover (`hardware_esp32/`)](#3-esp32-autonomous-hardware-rover-hardware_esp32)
- [Hardware Specifications & Pinout](#-hardware-specifications--pinout)
- [Track Topology & Route Navigation](#-track-topology--route-navigation)
- [Database Schema (Supabase)](#-database-schema-supabase)
- [Repository Structure](#-repository-structure)
- [Getting Started & Installation](#-getting-started--installation)
  - [Prerequisites](#prerequisites)
  - [1. Web Portal Setup](#1-web-portal-setup)
  - [2. Digital Twin Setup](#2-digital-twin-setup)
  - [3. ESP32 Firmware Setup](#3-esp32-firmware-setup)
  - [4. Netlify Deployment](#4-netlify-deployment)
- [Firmware Serial CLI & PID Tuning](#-firmware-serial-cli--pid-tuning)
- [Offline Resilience & Sync Engine](#-offline-resilience--sync-engine)
- [Roadmap & Future Improvements](#-roadmap--future-improvements)

---

## 🌟 System Overview

**Autonomous MedRover** is designed for indoor healthcare facilities to autonomously transport medicines, lab specimens, and sterile supplies directly to patient rooms. 

Instead of requiring expensive SLAM lidar platforms, MedRover utilizes a high-reliability **5-channel IR line-following architecture** with real-time junction arbitration, motor kick compensation, wobble recovery, and automated U-turn return-to-dock logic. The entire system is linked via Supabase cloud infrastructure to:

1. **Staff Web Portal**: Allows nurses and hospital staff to place medicine orders, select target wards (Rooms A, B, C), manage pharmacy inventories, and track active deliveries.
2. **3D Digital Twin**: Renders a live Three.js virtual model of the hospital floor and rover, syncing live coordinates, yaw, battery, and Wi-Fi RSSI over Supabase Realtime channels.
3. **Hardware Rover**: An ESP32 microcontroller with DC gear motors and IR sensors that autonomously polls pending tasks, navigates to target rooms, waits for unloading, and returns home.

---

## 🏗 Architecture & Dataflow

```mermaid
flowchart TD
    subgraph Hospital Staff & Admin
        Staff[Hospital Staff / Nurse]
        Admin[Pharmacy Admin]
    end

    subgraph Web Portal (React + Vite)
        UI[Catalog & Cart UI]
        RoomSelect[Room Selector: A, B, C]
        LiveOrders[Live Orders Dashboard]
        AdminPanel[Admin Inventory & Dispatch Panel]
        SyncEngine[Offline Sync Queue & Local Cache]
    end

    subgraph Cloud Backend (Supabase)
        DB_Orders[(orders Table)]
        DB_Items[(order_items Table)]
        DB_Rover[(rover_status Table)]
        RT[Supabase Realtime WebSockets]
    end

    subgraph Digital Twin (React + Three.js)
        Scene3D[3D Hospital Corridor Scene]
        Rover3D[Rover 3D Mesh + Sensors]
        HUD[Telemetry HUD: Battery, RSSI, Yaw, X/Z]
        CamDirector[Multi-Camera Director: Free / Top-Down / Follow]
    end

    subgraph Physical Rover (ESP32)
        ESP32[ESP32 Microcontroller]
        IR5[5x TCRT5000 IR Sensor Array]
        PD[PD Line Follow + Junction Engine]
        L298N[Motor Driver + DC Gear Motors]
        SerialCLI[Interactive Serial Calibration CLI]
    end

    Staff --> UI --> RoomSelect --> SyncEngine
    Admin --> AdminPanel --> DB_Orders
    SyncEngine -->|REST / Realtime| DB_Orders
    SyncEngine -->|REST| DB_Items

    DB_Orders <-->|HTTP Polling: pending / in_transit / delivered| ESP32
    ESP32 -->|IR Line Guidance| IR5 --> PD --> L298N
    ESP32 -.->|Serial CLI 115200| SerialCLI

    DB_Orders -->|Realtime Events| RT
    DB_Rover -->|Realtime Events| RT
    RT --> LiveOrders
    RT --> Scene3D
    RT --> HUD
    Rover3D <--> Scene3D
```

---

## ⚡ Key Features by Subsystem

### 1. Web Portal (`web_portal/`)
- **Modern Healthcare UI**: Built with React 18, Vite, Bootstrap 5, and Bootstrap Icons.
- **Role & Auth Flow**: Supabase Auth support (login, registration, guest browsing mode, and protected checkout routes).
- **Medicine Directory**: Interactive search catalog (`/search`), category filters, dosage info, stock availability, and dynamic cart badges.
- **Ward / Room Selection**: Visual room dispatch interface (`/select-room`) routing deliveries to Room A (General), Room B (ICU), or Room C (Pediatric).
- **Admin Command Center (`/admin`)**:
  - Live inventory management (edit prices, update stock counts, add new medicines).
  - Complete order history with real-time status badges (`pending`, `in_transit`, `delivered`, `cancelled`).
  - Manual dispatch overrides and single-click order status toggling.
- **Offline Resilience (`syncQueue.js` & `deliveryCache.js`)**:
  - Automatically captures orders and operations in `localStorage` if Wi-Fi drops.
  - Displays a global `OfflineBanner.jsx` warning to hospital personnel.
  - Automatically flushes and synchronizes all queued operations as soon as connectivity is restored.

### 2. 3D Digital Twin Simulator (`digital_twin/`)
- **Real-Time 3D Hospital Visualization**: Powered by React 19, `@react-three/fiber`, and `@react-three/drei`.
- **Accurate Floorplan**: Modeled corridors, docking base, junction intersections (J1, J2, J3), and destination patient rooms (A, B, C).
- **Dynamic Rover Mesh**: Features functional rotating drive wheels, front caster, animated LIDAR scanner beam, and dual LED headlights with glowing guidance lines.
- **Supabase Realtime Sync**: Subscribes to `rover_status` and `orders` channels to sync virtual rover position with physical rover telemetry in real time.
- **Multi-Camera Director**:
  - **Free Orbit**: Full 3D pan, zoom, and tilt around the hospital.
  - **Top-Down (2D Map)**: Overhead floorplan view for mission oversight.
  - **Follow Rover**: 3rd-person chase camera locked behind the rover during transit.
- **Interactive Telemetry HUD**:
  - Displays battery voltage/percentage, Wi-Fi RSSI (dBm), (X, Z) coordinates, yaw angle, velocity, and mission progress.
  - One-click manual dispatch triggers (Room A, Room B, Room C), emergency stop, and return-to-base commands.
  - Built-in simulation toggle for testing without active hardware connected.

### 3. ESP32 Autonomous Hardware Rover (`hardware_esp32/`)
- **Primary Production Firmware**: `line_follow_v33_MainWorkingCode.ino`.
- **PD Line Following**: Calculates continuous weighted position error across 5 IR sensors with proportional ($K_p$) and derivative ($K_d$) steering correction.
- **Junction Arbitration**: Debounced multi-frame confirmation (`JUNCTION_FRAMES_NEEDED = 3`) to eliminate spurious triggers from floor seams.
- **Auto-Order Polling**: Connects to hospital Wi-Fi, queries Supabase `/rest/v1/orders?status=eq.pending`, locks the mission, transitions status to `in_transit`, and kicks off motor motion.
- **Movement Hardening (v33)**:
  - 220ms start kick pulse overcoming initial static friction.
  - Automatic watch-mode suppression before auto-run dispatch.
  - Return-path cooldown to prevent premature stops at intermediate junctions.
  - Blocking 180° spin and home-dock alignment upon return.
- **Interactive Serial CLI**: Full set of runtime commands at `115200` baud for sensor calibration and real-time PID tuning.

---

## 🔌 Hardware Specifications & Pinout

### Component List
- **MCU**: ESP32-WROOM-32 Development Board (38-pin or 30-pin)
- **Sensors**: 5-channel TCRT5000 IR reflectance sensor array
- **Actuators**: Dual 3-6V DC geared TT motors with rubber wheels + front omnidirectional caster
- **Driver**: L298N Dual H-Bridge Motor Driver
- **Power**: 2x 18650 Li-ion batteries (7.4V) with common ground to ESP32
- **Track**: 18-20mm matte black electrical tape on a high-contrast matte light floor

### ESP32 GPIO Pin Mapping

| Component | Pin Function | ESP32 GPIO | Description / Notes |
| :--- | :--- | :---: | :--- |
| **L298N Motor Driver** | `ENA` | **GPIO 14** | PWM speed control for Left Motor |
| | `IN1` | **GPIO 27** | Left Motor Direction A |
| | `IN2` | **GPIO 26** | Left Motor Direction B |
| | `IN3` | **GPIO 25** | Right Motor Direction A |
| | `IN4` | **GPIO 33** | Right Motor Direction B |
| | `ENB` | **GPIO 12** | PWM speed control for Right Motor |
| **5-Ch IR Sensor Array** | `S1` (Far Left) | **GPIO 34** | Junction & branch detection (Input only) |
| | `S2` (Mid Left) | **GPIO 35** | PD fine steering (Input only) |
| | `S3` (Center) | **GPIO 32** | Line lock / on-track reference |
| | `S4` (Mid Right) | **GPIO 18** | PD fine steering |
| | `S5` (Far Right) | **GPIO 19** | Junction & branch detection |
| **Power & Ground** | `GND` | **GND** | Common ground with L298N & Battery |

> ⚠️ **IMPORTANT**: ESP32 and motor driver **MUST share a common ground (GND)**. Otherwise, PWM signal reference floating will cause erratic motor behavior or brownouts.

---

## 🗺 Track Topology & Route Navigation

The hospital floor navigation utilizes a hierarchical junction tree layout:

```
[ Room C / Pediatric ]
         |
         |  (Main Spine continuation)
   [ Junction 2 ] ------------> [ Room B / ICU ]
         |
         |  (Main Spine)
   [ Junction 1 ] ------------> [ Room A / General Ward ]
         |
         |
   [ BASE / DOCK ]
```

- **Room A Route**: Follows main spine $\rightarrow$ detects Junction 1 $\rightarrow$ turns 90° LEFT $\rightarrow$ travels spur to Room A $\rightarrow$ delivers for `ROOM_WAIT_MS` (7 sec) $\rightarrow$ 180° U-turn $\rightarrow$ follows spur back $\rightarrow$ turns onto main spine $\rightarrow$ returns to Dock.
- **Room B Route**: Follows main spine $\rightarrow$ skips Junction 1 $\rightarrow$ detects Junction 2 $\rightarrow$ turns 90° LEFT $\rightarrow$ travels spur to Room B $\rightarrow$ delivers $\rightarrow$ 180° U-turn $\rightarrow$ passes Junction 1 using cooldown bypass $\rightarrow$ docks at Base.
- **Room C Route**: Follows main spine through all junctions until reaching the terminal stop line (`allDark` condition) $\rightarrow$ delivers $\rightarrow$ 180° U-turn $\rightarrow$ returns straight down the spine back to Base.

---

## 🗄 Database Schema (Supabase)

The system relies on PostgreSQL hosted on Supabase with Realtime publication enabled.

### 1. `orders` Table
```sql
create table public.orders (
  id uuid default gen_random_uuid() primary key,
  user_id uuid references auth.users(id) on delete set null,
  room_code text not null,                -- 'A', 'B', or 'C'
  room_label text,                       -- e.g. 'General Ward', 'ICU', 'Pediatrics'
  status text not null default 'pending', -- 'pending', 'in_transit', 'delivered', 'cancelled'
  created_at timestamp with time zone default timezone('utc'::text, now()) not null
);

-- Enable Realtime
alter publication supabase_realtime add table public.orders;
```

### 2. `order_items` Table
```sql
create table public.order_items (
  id uuid default gen_random_uuid() primary key,
  order_id uuid references public.orders(id) on delete cascade not null,
  medicine_id text not null,
  quantity integer not null default 1,
  unit_price numeric(10,2) default 0,
  mrp numeric(10,2) default 0
);
```

### 3. `rover_status` Table (Optional Live Telemetry Feed)
```sql
create table public.rover_status (
  id text primary key default 'rover-01',
  battery integer default 100,
  wifi_rssi integer default -60,
  state text default 'idle',             -- 'idle', 'in_transit', 'delivered', 'returning_to_base', 'error'
  target_room text,                      -- 'A', 'B', 'C'
  order_id uuid references public.orders(id),
  yaw numeric(6,2) default 0.0,
  pos_x numeric(6,2) default 0.0,
  pos_z numeric(6,2) default 0.0,
  speed numeric(6,2) default 0.0,
  updated_at timestamp with time zone default timezone('utc'::text, now()) not null
);

-- Enable Realtime
alter publication supabase_realtime add table public.rover_status;
```

---

## 📂 Repository Structure

```
Autonomous-MedRover/
├── README.md                               # Primary system documentation (this file)
├── netlify.toml                            # Netlify SPA build & redirect configuration
├── CODE_EXAMPLES.md                        # Quick reference code snippets & adaptations
├── JAVASCRIPT_LEARNING_GUIDE.md            # Modern JavaScript concepts used in the portal
│
├── web_portal/                             # React + Vite Healthcare Ordering Portal
│   ├── package.json                        # Dependencies (React 18, Bootstrap 5, Supabase-js)
│   ├── vite.config.js                      # Vite bundler config
│   ├── .env.example                        # Template for Supabase credentials
│   ├── src/
│   │   ├── main.jsx                        # React entry point
│   │   ├── App.jsx                         # React Router routes & auth provider
│   │   ├── components/
│   │   │   ├── OfflineBanner.jsx           # Network status alert banner
│   │   │   ├── ProtectedRoute.jsx          # Route guard for authenticated users
│   │   │   ├── Navbar.jsx                  # Top navigation bar
│   │   │   └── Footer.jsx                  # Footer component
│   │   ├── contexts/
│   │   │   └── AuthContext.jsx             # Supabase Auth user state provider
│   │   ├── lib/
│   │   │   ├── supabaseClient.js           # Supabase client initializer
│   │   │   ├── syncQueue.js                # Offline transaction queue & auto-sync
│   │   │   ├── deliveryCache.js            # Offline local storage cache for active orders
│   │   │   └── cartStorage.js              # Persistent cart helper
│   │   └── pages/
│   │       ├── HomePage.jsx                # Landing & quick delivery portal
│   │       ├── SearchPage.jsx              # Medicine inventory search & add-to-cart
│   │       ├── CartPage.jsx                # Cart item review & quantity adjustments
│   │       ├── SelectRoomPage.jsx          # Ward & room selection checkout step
│   │       ├── OrderSuccessPage.jsx        # Dispatch confirmation screen
│   │       ├── OrdersPage.jsx              # Active & past order status tracker
│   │       ├── AdminPage.jsx               # Pharmacy admin, stock editor & order override
│   │       └── LoginPage.jsx               # Staff & user login/registration
│
├── digital_twin/                           # Three.js 3D Simulation & Telemetry HUD
│   ├── package.json                        # Dependencies (React 19, Three.js, R3F, Drei)
│   ├── vite.config.js                      # Vite bundler config
│   ├── src/
│   │   ├── App.jsx                         # Main 3D viewport & HUD overlay container
│   │   ├── components/
│   │   │   ├── Hospital3DScene.jsx         # Lights, floor, camera management, canvas
│   │   │   ├── CorridorLayout.jsx          # 3D walls, floor grid, guidance tracks, rooms
│   │   │   ├── RoverModel.jsx              # 3D Rover geometry, wheels, LIDAR beam, LEDs
│   │   │   └── TelemetryHUD.jsx            # Floating telemetry status dashboard & controls
│   │   ├── hooks/
│   │   │   └── useRoverRealtime.js         # Supabase Realtime subscription & kinematic physics
│   │   └── lib/
│   │       └── supabaseClient.js           # Supabase client initializer
│
└── hardware_esp32/                         # ESP32 Arduino Firmware & Calibration Sketches
    ├── Readme.md                           # Hardware guide & file index
    ├── line_follow_v33_MainWorkingCode.ino # ★ MAIN PRODUCTION FIRMWARE (A/B/C routing + return)
    ├── line_follow_v32.ino                 # Previous stable route firmware
    ├── line_follow_v31.ino                 # Intermediate version with junction counting
    ├── line_follow_v30.ino                 # Base line follower
    └── calibration_v*.ino                  # Iterative sensor and motor calibration sketches
```

---

## 🚀 Getting Started & Installation

### Prerequisites
- **Node.js**: v18.0 or higher
- **npm** or **yarn**
- **Arduino IDE**: v2.0+ with **ESP32 by Espressif Systems** board package installed
- **Supabase Account**: A free project at [supabase.com](https://supabase.com)

---

### 1. Web Portal Setup

```bash
# Navigate to web_portal
cd web_portal

# Install frontend dependencies
npm install

# Copy environment template
cp .env.example .env
```

Configure your `.env` file with your Supabase credentials:
```env
VITE_SUPABASE_URL=https://your-project-id.supabase.co
VITE_SUPABASE_ANON_KEY=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

Start the development server:
```bash
npm run dev
```
The web portal will open at `http://localhost:5173`.

---

### 2. Digital Twin Setup

```bash
# Navigate to digital_twin
cd ../digital_twin

# Install 3D and simulation dependencies
npm install

# Create local environment config
cp ../web_portal/.env.example .env
```

Ensure `.env` in `digital_twin/` contains:
```env
VITE_SUPABASE_URL=https://your-project-id.supabase.co
VITE_SUPABASE_ANON_KEY=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...
```

Run the Digital Twin application:
```bash
npm run dev -- --port 5174
```
Access the 3D twin simulator at `http://localhost:5174`.

---

### 3. ESP32 Firmware Setup

1. Launch **Arduino IDE**.
2. Open `hardware_esp32/line_follow_v33_MainWorkingCode.ino`.
3. Install required libraries via **Library Manager**:
   - `ArduinoJson` (by Benoit Blanchon, v6 or v7)
   - `WiFi` and `HTTPClient` (bundled with ESP32 core)
4. Open the sketch and configure your network and Supabase credentials (lines 38–45):
   ```cpp
   const char* WIFI_SSID         = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD     = "YOUR_WIFI_PASSWORD";
   const char* SUPABASE_URL      = "https://your-project-id.supabase.co";
   const char* SUPABASE_ANON_KEY = "your-supabase-anon-key";
   ```
5. Connect your ESP32 board via micro-USB.
6. Select Board: **ESP32 Dev Module** (or your matching board) and select the corresponding **COM Port**.
7. Click **Upload**.
8. Open the **Serial Monitor** at baud rate **`115200`** to monitor Wi-Fi connection and live IR logs.

---

### 4. Netlify Deployment

The project root contains `netlify.toml` pre-configured to build the `web_portal`:

1. Connect your repository to [Netlify](https://netlify.com).
2. Netlify will auto-detect the root `netlify.toml`:
   - **Base directory**: `web_portal`
   - **Build command**: `npm run build`
   - **Publish directory**: `web_portal/dist`
3. Under **Site Configuration $\rightarrow$ Environment Variables**, configure:
   - `VITE_SUPABASE_URL`
   - `VITE_SUPABASE_ANON_KEY`
4. Trigger deploy. Netlify handles SPA route rewrites to `/index.html` automatically.

---

## 🎛 Firmware Serial CLI & PID Tuning

When connected over Serial (`115200` baud), the rover accepts single-key commands in real time without requiring re-flashing:

| Key | Action / Description |
| :---: | :--- |
| `A` / `B` / `C` | Manually queue mission target to Room A, B, or C |
| `g` | **Go**: Start mission for the selected room |
| `s` | **Stop**: Immediate motor emergency stop |
| `p` | **Print Settings**: Dump current speeds, PID gains, and timing constants |
| `i` | **IR Snapshot**: Print instantaneous 5-sensor binary reading (e.g., `01110`) |
| `w` | **Watch Mode**: Stream live sensor readings continuously |
| `+` / `-` | Increase / decrease base forward speed |
| `]` / `[` | Increase / decrease Proportional gain ($K_p$) |
| `>` / `<` | Increase / decrease Derivative gain ($K_d$) |
| `)` / `(` | Increase / decrease turn speed |
| `n` / `m` | Adjust branch travel nudge time ($\pm 50\text{ ms}$) |

### Key Tuning Constants (`line_follow_v33_MainWorkingCode.ino`)
- `baseSpeed` (Default: `110`): Standard straight line velocity.
- `turnSpeed` (Default: `140`): Speed applied during 90° turns.
- `rightOffset` (Default: `120`): Calibrates motor differential imbalance.
- `ROOM_WAIT_MS` (Default: `7000`): Delivery drop-off dwell time before starting return U-turn.
- `RETURN_MIN_TRAVEL_MS` (Default: `800`): Suppresses premature junction detection right after U-turn.
- `JUNCTION_COOLDOWN_MS` (Default: `900`): Blanking period after passing a junction.

---

## 🛡 Offline Resilience & Sync Engine

Hospital Wi-Fi can experience dead spots. MedRover implements an offline-first architecture via `web_portal/src/lib/syncQueue.js`:

1. **Network Interception**: When staff places an order or updates status, the portal checks `navigator.onLine`.
2. **Local Queuing**: If offline or Supabase is temporarily unreachable, the transaction is given a unique transaction ID and stored in browser `localStorage` (`medrover_sync_queue`).
3. **Optimistic UI**: The staff interface updates immediately so hospital workflow is uninterrupted.
4. **Auto-Replay**: As soon as the browser receives the `online` event, the queue processor iterates over stored actions, executing updates with exponential retry logic.

---

## 🔮 Roadmap & Future Improvements

- [ ] **Ultrasonic / ToF Obstacle Braking**: Add HC-SR04 or VL53L0X sensor for dynamic collision avoidance.
- [ ] **Two-Way Telemetry Streaming from ESP32**: Push actual battery ADC voltage and RSSI to `rover_status` via lightweight HTTP/WebSocket.
- [ ] **RFID Cargo Compartment Lock**: Require RFID badge scan at room destination before opening medication hatch.
- [ ] **Automated Multi-Rover Fleet Scheduling**: Assign orders to the nearest available rover when scaling to multiple units.
- [ ] **OTA (Over-The-Air) Firmware Updates**: Update ESP32 sketches over Wi-Fi without physical USB tethering.

---

## 📄 License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
