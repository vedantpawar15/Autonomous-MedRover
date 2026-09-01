import React, { useRef } from 'react'
import { useFrame } from '@react-three/fiber'
import { Html } from '@react-three/drei'
import * as THREE from 'three'

export function RoverModel({ targetPosition = [0, 0.3, 0], yaw = 0, state = 'idle', battery = 95 }) {
  const groupRef = useRef()
  const lidarRef = useRef()
  const wheelRefs = useRef([])

  // State color mapping
  const getStateColor = () => {
    switch (state) {
      case 'in_transit':
        return '#3b82f6' // Glowing blue
      case 'delivered':
        return '#10b981' // Green
      case 'returning_to_base':
        return '#f59e0b' // Amber/Yellow
      case 'error':
        return '#ef4444' // Red
      case 'idle':
      default:
        return '#10b981' // Green
    }
  }

  useFrame((_, delta) => {
    if (!groupRef.current) return

    // Smooth Lerp Position (Interpolation speed)
    groupRef.current.position.x = THREE.MathUtils.lerp(groupRef.current.position.x, targetPosition[0], delta * 4)
    groupRef.current.position.y = THREE.MathUtils.lerp(groupRef.current.position.y, targetPosition[1], delta * 4)
    groupRef.current.position.z = THREE.MathUtils.lerp(groupRef.current.position.z, targetPosition[2], delta * 4)

    // Target Rotation angle in radians
    const targetRad = (yaw * Math.PI) / 180
    // Smooth Lerp Yaw Rotation
    let diff = targetRad - groupRef.current.rotation.y
    // Normalize diff to -PI .. PI
    diff = Math.atan2(Math.sin(diff), Math.cos(diff))
    groupRef.current.rotation.y += diff * delta * 5

    // Rotate LiDAR dome continuous rotation
    if (lidarRef.current) {
      lidarRef.current.rotation.y += delta * 4
    }

    // Spin wheels when in transit or returning
    if (state === 'in_transit' || state === 'returning_to_base') {
      wheelRefs.current.forEach((wheel) => {
        if (wheel) wheel.rotation.x += delta * 10
      })
    }
  })

  const beaconColor = getStateColor()

  return (
    <group ref={groupRef} position={targetPosition}>
      {/* ── MAIN ROVER CHASSIS (Robotic Metallic White Body) ── */}
      <mesh castShadow position={[0, 0.15, 0]}>
        <boxGeometry args={[0.7, 0.25, 1.0]} />
        <meshStandardMaterial color="#f8fafc" roughness={0.2} metalness={0.8} />
      </mesh>

      {/* Blue / Neon Side Trim Strips */}
      <mesh position={[0.36, 0.15, 0]}>
        <boxGeometry args={[0.02, 0.08, 0.9]} />
        <meshStandardMaterial color="#0284c7" emissive="#0284c7" emissiveIntensity={0.8} />
      </mesh>
      <mesh position={[-0.36, 0.15, 0]}>
        <boxGeometry args={[0.02, 0.08, 0.9]} />
        <meshStandardMaterial color="#0284c7" emissive="#0284c7" emissiveIntensity={0.8} />
      </mesh>

      {/* ── MEDICINE DELIVERY LOCKER / TRAY ON TOP ── */}
      <mesh castShadow position={[0, 0.32, 0.05]}>
        <boxGeometry args={[0.55, 0.18, 0.6]} />
        <meshStandardMaterial color="#0f172a" roughness={0.3} />
      </mesh>
      {/* Locker Glass Cover */}
      <mesh position={[0, 0.42, 0.05]}>
        <boxGeometry args={[0.5, 0.04, 0.55]} />
        <meshStandardMaterial color="#38bdf8" opacity={0.5} transparent roughness={0.1} />
      </mesh>

      {/* ── ROTATING LIDAR / ULTRASONIC DOME ── */}
      <group position={[0, 0.45, -0.3]}>
        {/* Base Mount */}
        <mesh>
          <cylinderGeometry args={[0.08, 0.1, 0.06, 16]} />
          <meshStandardMaterial color="#334155" />
        </mesh>
        {/* Revolving Turret Sensor */}
        <mesh ref={lidarRef} position={[0, 0.05, 0]}>
          <cylinderGeometry args={[0.07, 0.07, 0.08, 16]} />
          <meshStandardMaterial color="#0284c7" emissive="#0284c7" emissiveIntensity={1} />
        </mesh>
      </group>

      {/* ── STATUS LIGHT BEACON (GLOWING TOP TOWER) ── */}
      <group position={[0, 0.48, 0.3]}>
        <mesh>
          <cylinderGeometry args={[0.04, 0.04, 0.1, 16]} />
          <meshStandardMaterial color={beaconColor} emissive={beaconColor} emissiveIntensity={2.0} />
        </mesh>
        <pointLight color={beaconColor} intensity={2} distance={2} />
      </group>

      {/* ── FRONT BUMPER & 5x IR SENSOR ARRAY (S1 - S5) ── */}
      <group position={[0, 0.06, -0.52]}>
        {/* Bumper Mount */}
        <mesh>
          <boxGeometry args={[0.65, 0.06, 0.05]} />
          <meshStandardMaterial color="#1e293b" />
        </mesh>
        {/* 5 IR Sensor LEDs */}
        {[-0.25, -0.125, 0, 0.125, 0.25].map((xOffset, i) => (
          <mesh key={i} position={[xOffset, 0, -0.03]}>
            <sphereGeometry args={[0.025, 12, 12]} />
            <meshStandardMaterial color="#ef4444" emissive="#ef4444" emissiveIntensity={1.5} />
          </mesh>
        ))}
      </group>

      {/* ── 4 WHEELS WITH HUBCAPS ── */}
      {/* Front-Left Wheel */}
      <Wheel ref={(el) => (wheelRefs.current[0] = el)} position={[-0.38, 0.08, -0.3]} />
      {/* Front-Right Wheel */}
      <Wheel ref={(el) => (wheelRefs.current[1] = el)} position={[0.38, 0.08, -0.3]} />
      {/* Rear-Left Wheel */}
      <Wheel ref={(el) => (wheelRefs.current[2] = el)} position={[-0.38, 0.08, 0.3]} />
      {/* Rear-Right Wheel */}
      <Wheel ref={(el) => (wheelRefs.current[3] = el)} position={[0.38, 0.08, 0.3]} />

      {/* ── OVERHEAD FLOATING NAME BADGE ── */}
      <Html position={[0, 0.75, 0]} center distanceFactor={10}>
        <div className="flex items-center gap-1.5 bg-slate-900/90 text-white text-xs px-2.5 py-1 rounded-full border border-cyan-500/50 shadow-lg backdrop-blur-md whitespace-nowrap select-none">
          <span className="w-2 h-2 rounded-full animate-ping" style={{ backgroundColor: beaconColor }} />
          <span className="font-semibold tracking-wider text-cyan-400">MEDROVER #01</span>
          <span className="text-[10px] text-slate-400">({state.toUpperCase()})</span>
        </div>
      </Html>
    </group>
  )
}

const Wheel = React.forwardRef(({ position }, ref) => {
  return (
    <group ref={ref} position={position} rotation={[0, 0, Math.PI / 2]}>
      {/* Tire Rubber */}
      <mesh castShadow>
        <cylinderGeometry args={[0.12, 0.12, 0.08, 24]} />
        <meshStandardMaterial color="#090d16" roughness={0.8} />
      </mesh>
      {/* Hubcap */}
      <mesh position={[0, 0.041, 0]}>
        <cylinderGeometry args={[0.06, 0.06, 0.002, 16]} />
        <meshStandardMaterial color="#38bdf8" />
      </mesh>
    </group>
  )
})
