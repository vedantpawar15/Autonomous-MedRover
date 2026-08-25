import React from 'react'
import {
  Battery,
  Wifi,
  Compass,
  MapPin,
  Activity,
  Play,
  RotateCcw,
  OctagonAlert,
  Eye,
  Camera,
  Layers,
  Radio,
  Box,
  CheckCircle2,
  AlertTriangle
} from 'lucide-react'

export function TelemetryHUD({
  roverState,
  onDispatch,
  onReturnBase,
  onEmergencyStop,
  cameraMode,
  setCameraMode,
  isRealtimeConnected,
  isSimulating,
  setIsSimulating
}) {
  const { position, yaw, state, activeRoom, battery, wifiRssi, orderId, speed, progress } = roverState

  const getStatusBadge = () => {
    switch (state) {
      case 'in_transit':
        return { text: 'IN TRANSIT', bg: 'bg-blue-500/20 text-blue-400 border-blue-500/40' }
      case 'delivered':
        return { text: 'DELIVERED', bg: 'bg-emerald-500/20 text-emerald-400 border-emerald-500/40' }
      case 'returning_to_base':
        return { text: 'RETURNING TO BASE', bg: 'bg-amber-500/20 text-amber-400 border-amber-500/40' }
      case 'error':
        return { text: 'ERROR ALARM', bg: 'bg-red-500/20 text-red-400 border-red-500/40' }
      case 'idle':
      default:
        return { text: 'IDLE AT BASE', bg: 'bg-emerald-500/20 text-emerald-400 border-emerald-500/40' }
    }
  }

  const badge = getStatusBadge()

  return (
    <div className="absolute inset-0 pointer-events-none flex flex-col justify-between p-4 sm:p-6 select-none overflow-hidden">
      {/* ── TOP HEADER BAR ── */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 pointer-events-auto">
        {/* Title */}
        <div className="flex items-center gap-3 bg-slate-900/80 border border-slate-800 backdrop-blur-md px-4 py-2.5 rounded-xl shadow-xl">
          <div className="w-9 h-9 rounded-lg bg-cyan-500/10 border border-cyan-500/30 flex items-center justify-center text-cyan-400">
            <Box className="w-5 h-5 animate-pulse" />
          </div>
          <div>
            <h1 className="text-sm font-bold text-white tracking-wide uppercase flex items-center gap-2">
              MedRover Digital Twin
              <span className="text-[10px] font-normal px-2 py-0.5 rounded bg-cyan-500/20 text-cyan-300 border border-cyan-500/30">
                v2.5
              </span>
            </h1>
            <p className="text-xs text-slate-400">Low-Cost IR Delivery Robot Twin</p>
          </div>
        </div>

        {/* Realtime Connection Status & Camera Controls */}
        <div className="flex items-center gap-2">
          {/* Connection Status */}
          <div className="flex items-center gap-2 bg-slate-900/80 border border-slate-800 backdrop-blur-md px-3 py-2 rounded-xl text-xs font-medium">
            <Radio className={`w-4 h-4 ${isRealtimeConnected ? 'text-emerald-400 animate-pulse' : 'text-amber-400'}`} />
            <span className="text-slate-300">
              {isRealtimeConnected ? 'Supabase Realtime' : 'Local Stream'}
            </span>
            <span className={`w-2 h-2 rounded-full ${isRealtimeConnected ? 'bg-emerald-500' : 'bg-amber-500'}`} />
          </div>

          {/* Camera View Switcher */}
          <div className="flex items-center bg-slate-900/90 border border-slate-800 backdrop-blur-md p-1 rounded-xl shadow-xl">
            <button
              onClick={() => setCameraMode('free')}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'free'
                  ? 'bg-cyan-500 text-slate-950 shadow-lg shadow-cyan-500/20'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Eye className="w-3.5 h-3.5" />
              3D Orbit
            </button>
            <button
              onClick={() => setCameraMode('topdown')}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'topdown'
                  ? 'bg-cyan-500 text-slate-950 shadow-lg shadow-cyan-500/20'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Layers className="w-3.5 h-3.5" />
              Top Map
            </button>
            <button
              onClick={() => setCameraMode('follow')}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all ${
                cameraMode === 'follow'
                  ? 'bg-cyan-500 text-slate-950 shadow-lg shadow-cyan-500/20'
                  : 'text-slate-400 hover:text-white'
              }`}
            >
              <Camera className="w-3.5 h-3.5" />
              Follow
            </button>
          </div>
        </div>
      </div>

      {/* ── MIDDLE / MAIN TELEMETRY CARDS (FLOATING LEFT PANEL) ── */}
      <div className="flex flex-col sm:flex-row items-start justify-between gap-4 pointer-events-auto mt-4 sm:mt-0">
        {/* Left Telemetry Box */}
        <div className="w-full sm:w-80 bg-slate-900/85 border border-slate-800/90 backdrop-blur-xl p-4 rounded-2xl shadow-2xl space-y-4">
          {/* Status Header */}
          <div className="flex items-center justify-between border-b border-slate-800 pb-3">
            <span className="text-xs font-semibold text-slate-400 tracking-wider">ROVER TELEMETRY</span>
            <span className={`text-[11px] font-bold px-2.5 py-1 rounded-full border ${badge.bg}`}>
              {badge.text}
            </span>
          </div>

          {/* Target & Order info */}
          <div className="grid grid-cols-2 gap-2">
            <div className="bg-slate-950/60 p-2.5 rounded-xl border border-slate-800/60">
              <span className="text-[10px] font-medium text-slate-400 block uppercase">Target Ward</span>
              <span className="text-sm font-bold text-cyan-400 flex items-center gap-1.5 mt-0.5">
                <MapPin className="w-4 h-4 text-cyan-400" />
                {activeRoom ? `Room ${activeRoom}` : 'Base Dock'}
              </span>
            </div>
            <div className="bg-slate-950/60 p-2.5 rounded-xl border border-slate-800/60">
              <span className="text-[10px] font-medium text-slate-400 block uppercase">Order Reference</span>
              <span className="text-xs font-mono font-bold text-slate-200 block truncate mt-1">
                {orderId ? `#${orderId.toString().slice(0, 8)}` : 'None'}
              </span>
            </div>
          </div>

          {/* Battery & WiFi Meters */}
          <div className="space-y-3">
            {/* Battery Level */}
            <div>
              <div className="flex justify-between text-xs mb-1">
                <span className="text-slate-400 flex items-center gap-1">
                  <Battery className="w-3.5 h-3.5 text-emerald-400" />
                  Battery Level
                </span>
                <span className="font-bold text-slate-200">{battery}%</span>
              </div>
              <div className="w-full bg-slate-950 h-2 rounded-full overflow-hidden border border-slate-800">
                <div
                  className={`h-full transition-all duration-500 rounded-full ${
                    battery > 60
                      ? 'bg-emerald-500'
                      : battery > 20
                      ? 'bg-amber-500'
                      : 'bg-red-500'
                  }`}
                  style={{ width: `${battery}%` }}
                />
              </div>
            </div>

            {/* Wi-Fi RSSI Signal */}
            <div className="flex items-center justify-between text-xs bg-slate-950/40 px-3 py-2 rounded-xl border border-slate-800/40">
              <span className="text-slate-400 flex items-center gap-1.5">
                <Wifi className="w-3.5 h-3.5 text-cyan-400" />
                Wi-Fi Signal
              </span>
              <span className="font-mono font-semibold text-slate-200">{wifiRssi} dBm</span>
            </div>
          </div>

          {/* IMU Orientation & Coordinates */}
          <div className="grid grid-cols-2 gap-2 text-xs">
            <div className="bg-slate-950/60 p-2 rounded-xl border border-slate-800/60 flex items-center gap-2">
              <Compass className="w-4 h-4 text-cyan-400" />
              <div>
                <span className="text-[10px] text-slate-400 block">IMU Yaw Angle</span>
                <span className="font-mono font-bold text-white">{Math.round(yaw)}°</span>
              </div>
            </div>
            <div className="bg-slate-950/60 p-2 rounded-xl border border-slate-800/60 flex items-center gap-2">
              <Activity className="w-4 h-4 text-blue-400" />
              <div>
                <span className="text-[10px] text-slate-400 block">Grid Pos (X, Z)</span>
                <span className="font-mono font-bold text-white">
                  {position[0].toFixed(1)}, {position[2].toFixed(1)}
                </span>
              </div>
            </div>
          </div>

          {/* Mission Progress */}
          {state !== 'idle' && (
            <div>
              <div className="flex justify-between text-xs mb-1">
                <span className="text-slate-400">Mission Path Progress</span>
                <span className="font-bold text-cyan-400">{Math.round(progress)}%</span>
              </div>
              <div className="w-full bg-slate-950 h-1.5 rounded-full overflow-hidden border border-slate-800">
                <div
                  className="bg-cyan-500 h-full transition-all duration-300 rounded-full"
                  style={{ width: `${progress}%` }}
                />
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ── BOTTOM CONTROL TOOLBAR ── */}
      <div className="w-full pointer-events-auto flex flex-col sm:flex-row items-center justify-between gap-3 bg-slate-900/90 border border-slate-800/90 backdrop-blur-xl p-3 sm:p-4 rounded-2xl shadow-2xl">
        {/* Dispatch Room Buttons */}
        <div className="flex items-center gap-2 flex-wrap">
          <span className="text-xs font-semibold text-slate-400 mr-1 hidden sm:inline">DISPATCH MISSION:</span>
          
          <button
            onClick={() => onDispatch('A')}
            disabled={state === 'in_transit'}
            className="flex items-center gap-1.5 px-3 py-2 bg-blue-600/20 hover:bg-blue-600/30 text-blue-300 border border-blue-500/40 rounded-xl text-xs font-bold transition-all disabled:opacity-50"
          >
            <Play className="w-3.5 h-3.5 text-blue-400" />
            Room A (General)
          </button>

          <button
            onClick={() => onDispatch('B')}
            disabled={state === 'in_transit'}
            className="flex items-center gap-1.5 px-3 py-2 bg-pink-600/20 hover:bg-pink-600/30 text-pink-300 border border-pink-500/40 rounded-xl text-xs font-bold transition-all disabled:opacity-50"
          >
            <Play className="w-3.5 h-3.5 text-pink-400" />
            Room B (ICU)
          </button>

          <button
            onClick={() => onDispatch('C')}
            disabled={state === 'in_transit'}
            className="flex items-center gap-1.5 px-3 py-2 bg-purple-600/20 hover:bg-purple-600/30 text-purple-300 border border-purple-500/40 rounded-xl text-xs font-bold transition-all disabled:opacity-50"
          >
            <Play className="w-3.5 h-3.5 text-purple-400" />
            Room C (Pediatric)
          </button>
        </div>

        {/* Rover Action Buttons */}
        <div className="flex items-center gap-2">
          <button
            onClick={onReturnBase}
            disabled={state === 'idle'}
            className="flex items-center gap-1.5 px-3.5 py-2 bg-amber-500/20 hover:bg-amber-500/30 text-amber-300 border border-amber-500/40 rounded-xl text-xs font-bold transition-all disabled:opacity-50"
          >
            <RotateCcw className="w-3.5 h-3.5" />
            Return to Base
          </button>

          <button
            onClick={onEmergencyStop}
            className="flex items-center gap-1.5 px-3.5 py-2 bg-red-600/20 hover:bg-red-600/30 text-red-300 border border-red-500/40 rounded-xl text-xs font-bold transition-all"
          >
            <OctagonAlert className="w-3.5 h-3.5 text-red-400" />
            E-STOP
          </button>

          {/* Simulation Toggle Button */}
          <button
            onClick={() => setIsSimulating(!isSimulating)}
            className={`flex items-center gap-1.5 px-3 py-2 rounded-xl text-xs font-semibold border transition-all ${
              isSimulating
                ? 'bg-emerald-500/20 border-emerald-500/50 text-emerald-300'
                : 'bg-slate-800/60 border-slate-700 text-slate-400'
            }`}
          >
            <Activity className="w-3.5 h-3.5" />
            {isSimulating ? 'Simulating Path' : 'Enable Sim'}
          </button>
        </div>
      </div>
    </div>
  )
}
