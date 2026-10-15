import React, { useState } from 'react'
import {
  RotateCcw,
  Play,
  Pause,
  OctagonAlert,
  ArrowUp,
  Clock,
  Gauge,
  PackageCheck,
  Cpu,
  Layers,
  Camera,
  Eye,
  Activity,
  MapPin,
  BatteryCharging,
  Wifi,
  Compass,
  ChevronRight,
  ChevronLeft,
  Box,
  X
} from 'lucide-react'
import { ROOMS_CONFIG } from './CorridorLayout'

export function TelemetryHUD({
  roverState,
  esp32Sensors,
  onDispatch,
  onResetView,
  onReturnBase,
  onEmergencyStop,
  onResumeMission,
  isPaused,
  speedMultiplier,
  setSpeedMultiplier,
  cameraMode,
  setCameraMode,
  isRealtimeConnected,
  hardwareSyncMode,
  setHardwareSyncMode
}) {
  const [showTelemetryCard, setShowTelemetryCard] = useState(true)

  const {
    state = 'idle',
    activeRoom,
    activeRoomName,
    speed = 0,
    etaSeconds = 0,
    distanceRemaining = 0,
    distanceTravelled = 0,
    progress = 0,
    guidance = 'Proceed straight on Central Corridor toward Junction J2',
    battery = 33,
    wifiRssi = -58,
    yaw = 180,
    orderId = 151,
    position = [0, 0.2, 0]
  } = roverState || {}

  const {
    temperature = 31.3,
    humidity = 41,
    gasMq = 531,
    distance = 33.88,
    syncedTime = '7:04:58 PM'
  } = esp32Sensors || {}

  const getNavIcon = () => {
    if (state === 'delivered') return <PackageCheck className="w-5 h-5 text-emerald-400 animate-bounce" />
    if (state === 'returning_to_base') return <RotateCcw className="w-5 h-5 text-amber-400" />
    return <ArrowUp className="w-5 h-5 text-cyan-400 animate-pulse" />
  }

  // Calculate percentage of route completed
  const progressPercent =
    state === 'delivered'
      ? 100
      : state === 'idle'
      ? 0
      : Math.min(99, Math.max(5, Math.round(progress * 100 || ((12.7 - (distanceRemaining || 0)) / 12.7) * 100 || 15)))

  // Calculate distance done
  const displayDone =
    state === 'delivered'
      ? '12.7 m'
      : state === 'idle'
      ? '0 m'
      : distanceTravelled > 0
      ? `${distanceTravelled.toFixed(1)} m`
      : '0 m'

  // Calculate remaining
  const displayRemaining =
    state === 'delivered'
      ? '0.0 m'
      : distanceRemaining > 0
      ? `${distanceRemaining.toFixed(1)} m`
      : state === 'idle'
      ? '12.7 m'
      : '12.7 m'

  return (
    <div className="absolute inset-0 pointer-events-none flex flex-col justify-between p-3 sm:p-5 select-none overflow-hidden font-sans">
      {/* ── TOP BAR SECTION: EXACT MATCH TO USER'S REFERENCE (media_1789047955478.png) ── */}
      <div className="flex items-center justify-between w-full gap-3 pointer-events-auto">
        {/* ── TOP-LEFT CARD ── */}
        <div className="flex items-center gap-3 bg-[#0b1329]/95 border border-slate-800/90 backdrop-blur-xl px-4 py-2.5 rounded-2xl shadow-xl">
          <div className="w-10 h-10 rounded-2xl bg-cyan-500/10 border border-cyan-500/30 flex items-center justify-center text-cyan-400 shrink-0">
            <Box className="w-5 h-5" />
          </div>
          <div>
            <div className="flex items-center gap-2.5">
              <h1 className="text-sm font-bold tracking-wide text-white uppercase font-sans">
                MEDROVER DIGITAL TWIN
              </h1>
              <span className="flex items-center gap-1 text-[10px] font-mono font-bold text-emerald-400 bg-emerald-950/70 border border-emerald-500/50 px-2 py-0.5 rounded-md">
                <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
                PORTAL CONNECTED
              </span>
            </div>
            <p className="text-xs text-slate-400 font-sans mt-0.5">
              Autonomous Line-Tracking Fleet &bull; Hardware Synced
            </p>
          </div>
        </div>

        {/* ── TOP-CENTER LIVE VEHICLE ARRIVAL NOTIFICATION ── */}
        <div className="flex items-center gap-3 bg-[#0b1329]/95 border border-slate-800/90 backdrop-blur-xl px-4 py-2.5 rounded-2xl shadow-xl max-w-lg">
          <div className="w-8 h-8 rounded-lg bg-slate-900 border border-cyan-500/30 flex items-center justify-center shrink-0 text-cyan-400">
            {getNavIcon()}
          </div>
          <div className="flex-1 min-w-0">
            <div className="text-xs font-bold text-white tracking-wide truncate">
              {state === 'in_transit' || state === 'delivering'
                ? `Proceeding to ${activeRoomName || 'Room B'}. MedRover en route...`
                : state === 'idle'
                ? 'MedRover Docked at Base. Ready for next mission.'
                : guidance}
            </div>
            <div className="flex items-center gap-2 text-[11px] text-slate-400 mt-0.5">
              <span className="text-cyan-400 font-semibold flex items-center gap-1">
                <Clock className="w-3 h-3" />
                {state === 'idle' ? 'Arriving in ~0s' : `Arriving in ~${etaSeconds || 32}s`}
              </span>
              <span>&bull;</span>
              <span className="text-slate-300 font-medium">
                {displayRemaining} away
              </span>
              <span>&bull;</span>
              <span>Main Spine Corridor</span>
            </div>
          </div>
          <div className="flex flex-col items-center pl-3 border-l border-slate-800">
            <span className="w-2.5 h-2.5 rounded-full bg-cyan-400 shadow-md shadow-cyan-400/50 animate-pulse" />
            <span className="text-[9px] font-mono text-cyan-300 font-bold mt-0.5">SYNCED</span>
          </div>
        </div>

        {/* ── TOP-RIGHT CONTROLS: Hardware Sync & Camera Modes (Screenshot 2 Match) ── */}
        <div className="flex items-center gap-2">
          <button
            onClick={() => setHardwareSyncMode(!hardwareSyncMode)}
            className="flex items-center gap-1.5 px-3 py-2 rounded-xl text-xs font-mono font-medium border backdrop-blur-md transition-all shadow-md bg-[#0b1329]/95 border-slate-800 text-slate-300 hover:text-white"
          >
            <Cpu className="w-3.5 h-3.5 text-cyan-400" />
            {hardwareSyncMode ? 'ESP32 Sync ON' : 'Hardware Sync OFF'}
          </button>

          <div className="flex items-center bg-[#0b1329]/95 border border-slate-800/90 backdrop-blur-md p-1 rounded-xl shadow-xl">
            <button
              onClick={() => setCameraMode('free')}
              className={`flex items-center gap-1 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'free'
                  ? 'bg-cyan-500 text-slate-950 font-bold shadow'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Eye className="w-3.5 h-3.5" />
              3D Orbit
            </button>
            <button
              onClick={() => setCameraMode('topdown')}
              className={`flex items-center gap-1 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'topdown'
                  ? 'bg-cyan-500 text-slate-950 font-bold shadow'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Layers className="w-3.5 h-3.5" />
              Top Map
            </button>
            <button
              onClick={() => setCameraMode('follow')}
              className={`flex items-center gap-1 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'follow'
                  ? 'bg-cyan-500 text-slate-950 font-bold shadow'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Camera className="w-3.5 h-3.5" />
              Follow
            </button>
          </div>
        </div>
      </div>

      {/* ── LEFT SIDE: ROVER TELEMETRY OPTION (Docked on Left) ── */}
      <div className="absolute left-4 sm:left-6 top-24 z-20 pointer-events-auto">
        {showTelemetryCard ? (
          <div className="w-80 max-h-[calc(100vh-180px)] overflow-y-auto bg-[#0b1329]/95 border border-slate-800/90 backdrop-blur-2xl p-4 rounded-3xl shadow-2xl space-y-3 text-slate-200 font-sans transition-all animate-in fade-in slide-in-from-left-4 duration-300">
            {/* Header */}
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <Activity className="w-4 h-4 text-cyan-400 animate-pulse" />
                <span className="text-xs font-mono font-bold tracking-wider text-slate-300 uppercase">
                  ROVER TELEMETRY
                </span>
              </div>
              <div className="flex items-center gap-1.5">
                <span className={`px-2.5 py-0.5 rounded-full text-[10px] font-mono font-bold border ${
                  state === 'in_transit' || state === 'delivering'
                    ? 'bg-[#082845] text-cyan-300 border-cyan-500/50'
                    : state === 'delivered'
                    ? 'bg-emerald-950/80 text-emerald-300 border-emerald-500/50'
                    : 'bg-slate-900 text-slate-300 border-slate-700'
                }`}>
                  {state === 'in_transit' || state === 'delivering'
                    ? 'IN TRANSIT (LIVE)'
                    : state === 'delivered'
                    ? 'DELIVERED (LIVE)'
                    : 'STANDBY (READY)'}
                </span>
                <button
                  onClick={() => setShowTelemetryCard(false)}
                  className="w-5 h-5 rounded-full bg-slate-800/80 hover:bg-slate-700 text-slate-400 hover:text-white flex items-center justify-center transition-all ml-1"
                  title="Minimize Rover Telemetry"
                >
                  <X className="w-3 h-3" />
                </button>
              </div>
            </div>

            {/* Travel Velocity Box */}
            <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-3">
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-1.5 text-slate-300 text-xs font-medium">
                  <Gauge className="w-4 h-4 text-cyan-400" />
                  <span>Travel Velocity</span>
                </div>
                <div className="flex items-baseline">
                  <span className="text-base font-bold text-white font-mono">{speed.toFixed(1)}</span>
                  <span className="text-xs text-cyan-400 font-bold ml-1">m/s</span>
                  <span className="text-[11px] text-slate-400 ml-1.5">({(speed * 3.6).toFixed(1)} km/h)</span>
                </div>
              </div>
              <div className="h-1.5 bg-slate-800 rounded-full overflow-hidden mt-2">
                <div
                  className="h-full bg-cyan-400 rounded-full transition-all duration-300"
                  style={{ width: `${Math.max(5, Math.min(100, (speed / 1.5) * 100))}%` }}
                />
              </div>
            </div>

            {/* 2x2 Grid: Destination, Order ID, Distance Done, Remaining */}
            <div className="grid grid-cols-2 gap-2">
              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2.5">
                <span className="text-[9px] font-mono font-bold text-slate-400 block uppercase tracking-wider">
                  DESTINATION
                </span>
                <div className="flex items-center gap-1 text-cyan-400 font-bold text-xs mt-1">
                  <MapPin className="w-3.5 h-3.5 shrink-0" />
                  <span className="truncate">{activeRoomName || 'Room B'}</span>
                </div>
              </div>

              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2.5">
                <span className="text-[9px] font-mono font-bold text-slate-400 block uppercase tracking-wider">
                  ORDER ID
                </span>
                <div className="text-white font-bold text-xs mt-1 font-mono">
                  #{orderId || 151}
                </div>
              </div>

              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2.5">
                <span className="text-[9px] font-mono text-slate-400 block">
                  Distance Done
                </span>
                <div className="text-white font-bold text-xs mt-1 font-mono">
                  {displayDone}
                </div>
              </div>

              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2.5">
                <span className="text-[9px] font-mono text-slate-400 block">
                  Remaining
                </span>
                <div className="text-cyan-400 font-bold text-xs mt-1 font-mono">
                  {displayRemaining}
                </div>
              </div>
            </div>

            {/* Battery Level */}
            <div>
              <div className="flex items-center justify-between text-xs">
                <div className="flex items-center gap-1.5 text-slate-300">
                  <BatteryCharging className="w-4 h-4 text-emerald-400" />
                  <span>Battery Level</span>
                </div>
                <span className="font-bold text-white font-mono">{Math.round(battery ?? 33)}%</span>
              </div>
              <div className="h-1.5 bg-slate-800 rounded-full overflow-hidden mt-1.5">
                <div
                  className="h-full bg-emerald-400 rounded-full transition-all duration-300"
                  style={{ width: `${Math.round(battery ?? 33)}%` }}
                />
              </div>
            </div>

            {/* Wi-Fi Signal */}
            <div className="flex items-center justify-between text-xs">
              <div className="flex items-center gap-1.5 text-slate-300">
                <Wifi className="w-4 h-4 text-cyan-400" />
                <span>Wi-Fi Signal</span>
              </div>
              <span className="font-bold text-white font-mono">{wifiRssi || -58} dBm</span>
            </div>

            {/* 2 Columns: Heading Yaw, Position X Z */}
            <div className="grid grid-cols-2 gap-2">
              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2">
                <div className="flex items-center gap-1 text-[10px] text-slate-400">
                  <Compass className="w-3 h-3 text-cyan-400" />
                  <span>Heading (Yaw)</span>
                </div>
                <div className="text-white font-bold text-xs mt-1 font-mono">
                  {Math.round(yaw || 180)}°
                </div>
              </div>

              <div className="bg-[#070d1d]/90 border border-slate-800/80 rounded-2xl p-2">
                <div className="flex items-center gap-1 text-[10px] text-slate-400">
                  <Activity className="w-3 h-3 text-cyan-400" />
                  <span>Position (X, Z)</span>
                </div>
                <div className="text-white font-bold text-xs mt-1 font-mono">
                  {position[0].toFixed(1)}, {position[2].toFixed(1)}
                </div>
              </div>
            </div>

            {/* Total Route Progress */}
            <div>
              <div className="flex items-center justify-between text-xs text-slate-400">
                <span>Total Route Progress</span>
                <span className="text-cyan-400 font-bold font-mono">
                  {progressPercent}%
                </span>
              </div>
              <div className="h-1 bg-slate-800 rounded-full overflow-hidden mt-1.5">
                <div
                  className="h-full bg-cyan-400 rounded-full transition-all duration-300"
                  style={{ width: `${progressPercent}%` }}
                />
              </div>
            </div>
          </div>
        ) : (
          /* Minimized Left-Side Option Tab */
          <button
            onClick={() => setShowTelemetryCard(true)}
            className="flex items-center gap-2 bg-[#0b1329]/95 border border-cyan-500/40 text-cyan-300 px-3.5 py-2 rounded-2xl shadow-2xl backdrop-blur-xl hover:bg-cyan-950/80 hover:text-white transition-all pointer-events-auto"
            title="Open Rover Telemetry Option"
          >
            <Activity className="w-4 h-4 text-cyan-400 animate-pulse" />
            <span className="text-xs font-mono font-bold tracking-wider uppercase">
              Rover Telemetry
            </span>
            <ChevronRight className="w-3.5 h-3.5" />
          </button>
        )}
      </div>

      {/* ── BOTTOM CONTROL TOOLBAR: EXACT MATCH TO USER'S REFERENCE (media_1789047963604.png) ── */}
      <div className="w-full pointer-events-auto flex items-center justify-between gap-3 bg-[#0b1329]/95 border border-slate-800/90 backdrop-blur-xl px-4 py-2.5 rounded-2xl shadow-2xl">
        {/* Left: SIMULATE / MANUAL DISPATCH: Room A, Room B, Room C */}
        <div className="flex items-center gap-3">
          <span className="text-[11px] font-bold text-slate-400 uppercase tracking-wider whitespace-nowrap">
            SIMULATE / MANUAL DISPATCH:
          </span>
          <button
            onClick={() => onDispatch('A')}
            className="px-3 py-1.5 rounded-xl text-xs font-bold border border-blue-500/40 bg-[#0c1f3d] text-blue-300 hover:bg-blue-500/20 transition-all flex items-center gap-1.5"
          >
            <Play className="w-3 h-3 fill-current" />
            Room A (General)
          </button>
          <button
            onClick={() => onDispatch('B')}
            className="px-3 py-1.5 rounded-xl text-xs font-bold border border-fuchsia-600/40 bg-[#2a1338] text-fuchsia-300 hover:bg-fuchsia-600/20 transition-all flex items-center gap-1.5"
          >
            <Play className="w-3 h-3 fill-current" />
            Room B (ICU)
          </button>
          <button
            onClick={() => onDispatch('C')}
            className="px-3 py-1.5 rounded-xl text-xs font-bold border border-purple-600/40 bg-[#21163e] text-purple-300 hover:bg-purple-600/20 transition-all flex items-center gap-1.5"
          >
            <Play className="w-3 h-3 fill-current" />
            Room C (Pediatric)
          </button>
        </div>

        {/* Right: 1x Real / 2x Fast, Pause, Reset Base, E-STOP */}
        <div className="flex items-center gap-2">
          {/* Speed Toggle (1x / 2x) */}
          <div className="flex items-center bg-slate-950/80 border border-slate-800 p-0.5 rounded-xl">
            <button
              onClick={() => setSpeedMultiplier(1)}
              className={`px-3 py-1 text-xs font-mono font-bold rounded-lg transition-all ${
                speedMultiplier === 1 ? 'bg-cyan-500 text-slate-950 shadow' : 'text-slate-400 hover:text-white'
              }`}
            >
              1x Real
            </button>
            <button
              onClick={() => setSpeedMultiplier(2)}
              className={`px-3 py-1 text-xs font-mono font-bold rounded-lg transition-all ${
                speedMultiplier === 2 ? 'bg-cyan-500 text-slate-950 shadow' : 'text-slate-400 hover:text-white'
              }`}
            >
              2x Fast
            </button>
          </div>

          {/* Pause / Resume */}
          <button
            onClick={isPaused ? onResumeMission : () => onEmergencyStop()}
            className={`flex items-center gap-1.5 px-3 py-1.5 rounded-xl text-xs font-mono font-bold border transition-all ${
              isPaused
                ? 'bg-emerald-500/20 text-emerald-300 border-emerald-500/40'
                : 'bg-[#141e33] text-slate-200 border-slate-700 hover:bg-slate-800'
            }`}
          >
            {isPaused ? <Play className="w-3 h-3" /> : <Pause className="w-3 h-3" />}
            {isPaused ? 'Resume' : 'Pause'}
          </button>

          {/* Reset Base */}
          <button
            onClick={onReturnBase}
            className="flex items-center gap-1.5 px-3 py-1.5 bg-[#261f0f] hover:bg-amber-500/20 text-amber-300 border border-amber-500/40 rounded-xl text-xs font-mono font-bold transition-all"
          >
            <RotateCcw className="w-3 h-3" />
            Reset Base
          </button>

          {/* E-Stop */}
          <button
            onClick={onEmergencyStop}
            className="flex items-center gap-1.5 px-3.5 py-1.5 bg-[#331118] hover:bg-red-600/30 text-red-300 border border-red-500/50 rounded-xl text-xs font-mono font-bold transition-all active:scale-95"
          >
            <OctagonAlert className="w-3 h-3 text-red-400" />
            E-STOP
          </button>
        </div>
      </div>
    </div>
  )
}

