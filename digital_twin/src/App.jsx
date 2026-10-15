import React, { useState } from 'react'
import { Hospital3DScene } from './components/Hospital3DScene'
import { TelemetryHUD } from './components/TelemetryHUD'
import { useRoverRealtime } from './hooks/useRoverRealtime'

export default function App() {
  const [cameraMode, setCameraMode] = useState('free') // free (CAD Orbit), walk (3D Game Walk), topdown (CAD Plan), follow (Rover)
  const [focusRoom, setFocusRoom] = useState(null)
  const [resetTrigger, setResetTrigger] = useState(0)
  const [isWireframe, setIsWireframe] = useState(false)
  const [cadCursor, setCadCursor] = useState({ x: 0, z: 1.5 })
  const [pressedKeys, setPressedKeys] = useState({
    w: false,
    a: false,
    s: false,
    d: false,
    shift: false,
    space: false
  })

  const {
    roverState,
    esp32Sensors,
    activeRoutePoints,
    dispatchMission,
    returnToBase,
    emergencyStop,
    resumeMission,
    isPaused,
    setIsPaused,
    speedMultiplier,
    setSpeedMultiplier,
    isRealtimeConnected,
    hardwareSyncMode,
    setHardwareSyncMode
  } = useRoverRealtime()

  const handleResetView = () => {
    setFocusRoom(null)
    setResetTrigger((t) => t + 1)
  }

  const handleSelectRoom = (roomCodeOrId) => {
    dispatchMission(roomCodeOrId)
  }

  return (
    <div className="w-screen h-screen overflow-hidden bg-[#d8e8fa] font-sans text-slate-100 select-none relative">
      {/* ── Interactive 3D Canvas Scene with WASD/Arrow game controls & CAD cursor ── */}
      <Hospital3DScene
        roverState={roverState}
        activeRoutePoints={activeRoutePoints}
        cameraMode={cameraMode}
        focusRoom={focusRoom}
        resetTrigger={resetTrigger}
        onSelectRoom={handleSelectRoom}
        isWireframe={isWireframe}
        onCadCursorMove={setCadCursor}
        onKeysChange={setPressedKeys}
      />

      {/* ── Floating Digital Twin Telemetry HUD Overlay (CAD & 3D Game HUD) ── */}
      <TelemetryHUD
        roverState={roverState}
        esp32Sensors={esp32Sensors}
        onDispatch={handleSelectRoom}
        onResetView={handleResetView}
        onReturnBase={() => returnToBase(false)}
        onEmergencyStop={emergencyStop}
        onResumeMission={resumeMission}
        isPaused={isPaused}
        speedMultiplier={speedMultiplier}
        setSpeedMultiplier={setSpeedMultiplier}
        cameraMode={cameraMode}
        setCameraMode={setCameraMode}
        isRealtimeConnected={isRealtimeConnected}
        hardwareSyncMode={hardwareSyncMode}
        setHardwareSyncMode={setHardwareSyncMode}
        isWireframe={isWireframe}
        setIsWireframe={setIsWireframe}
        cadCursor={cadCursor}
        pressedKeys={pressedKeys}
      />
    </div>
  )
}
