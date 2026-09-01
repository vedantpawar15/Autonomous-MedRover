import React, { useState } from 'react'
import { Hospital3DScene } from './components/Hospital3DScene'
import { TelemetryHUD } from './components/TelemetryHUD'
import { useRoverRealtime } from './hooks/useRoverRealtime'

export default function App() {
  const [cameraMode, setCameraMode] = useState('free') // free, topdown, follow
  const {
    roverState,
    dispatchMission,
    returnToBase,
    emergencyStop,
    isRealtimeConnected,
    isSimulating,
    setIsSimulating
  } = useRoverRealtime()

  return (
    <div className="w-screen h-screen overflow-hidden bg-slate-950 font-sans text-slate-100 select-none relative">
      {/* ── Interactive 3D Canvas Scene ── */}
      <Hospital3DScene roverState={roverState} cameraMode={cameraMode} />

      {/* ── Floating Digital Twin Telemetry HUD Overlay ── */}
      <TelemetryHUD
        roverState={roverState}
        onDispatch={(room) => dispatchMission(room)}
        onReturnBase={returnToBase}
        onEmergencyStop={emergencyStop}
        cameraMode={cameraMode}
        setCameraMode={setCameraMode}
        isRealtimeConnected={isRealtimeConnected}
        isSimulating={isSimulating}
        setIsSimulating={setIsSimulating}
      />
    </div>
  )
}
