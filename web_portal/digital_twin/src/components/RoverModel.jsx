import React, { useRef } from 'react'
import { useFrame } from '@react-three/fiber'
import { Html } from '@react-three/drei'
import * as THREE from 'three'

export function RoverModel({
  targetPosition = [0, 0.2, 0],
  yaw = 0,
  steeringAngle = 0,
  isLidOpen = false,
  state = 'idle',
  battery = 95,
  speed = 0,
  isWireframe = false
}) {
  const groupRef = useRef()
  const lidarRef = useRef()
  const wheelRefs = useRef([])
  const frontSteerRefs = useRef([])
  const lidRef = useRef()
  const pulseRingRef = useRef()

  const getStateColor = () => {
    switch (state) {
      case 'in_transit':
        return '#0284c7' // Clean medical blue
      case 'delivered':
        return '#10b981' // Clean emerald
      case 'returning_to_base':
        return '#f59e0b' // Amber
      case 'error':
        return '#ef4444' // Red
      case 'idle':
      default:
        return '#0284c7'
    }
  }

  useFrame((stateObj, delta) => {
    if (!groupRef.current) return

    groupRef.current.position.x = THREE.MathUtils.lerp(groupRef.current.position.x, targetPosition[0], Math.min(1, delta * 15))
    groupRef.current.position.y = THREE.MathUtils.lerp(groupRef.current.position.y, targetPosition[1], Math.min(1, delta * 15))
    groupRef.current.position.z = THREE.MathUtils.lerp(groupRef.current.position.z, targetPosition[2], Math.min(1, delta * 15))

    const targetRad = (yaw * Math.PI) / 180
    let diff = targetRad - groupRef.current.rotation.y
    diff = Math.atan2(Math.sin(diff), Math.cos(diff))
    groupRef.current.rotation.y += diff * Math.min(1, delta * 12)

    const targetSteerRad = (steeringAngle * Math.PI) / 180
    frontSteerRefs.current.forEach((steerGroup) => {
      if (steerGroup) {
        steerGroup.rotation.y = THREE.MathUtils.lerp(steerGroup.rotation.y, targetSteerRad, Math.min(1, delta * 10))
      }
    })

    if (lidarRef.current) {
      lidarRef.current.rotation.y += delta * 5
    }

    const spinDelta = speed > 0.05 ? speed * delta * 14 : (state === 'in_transit' || state === 'returning_to_base') ? delta * 8 : 0
    if (spinDelta > 0) {
      wheelRefs.current.forEach((wheel) => {
        if (wheel) wheel.rotation.x += spinDelta
      })
    }

    if (lidRef.current) {
      const targetLidY = isLidOpen ? 0.6 : 0.42
      const targetLidZ = isLidOpen ? -0.15 : 0.05
      lidRef.current.position.y = THREE.MathUtils.lerp(lidRef.current.position.y, targetLidY, delta * 5)
      lidRef.current.position.z = THREE.MathUtils.lerp(lidRef.current.position.z, targetLidZ, delta * 5)
    }

    if (pulseRingRef.current) {
      const t = (stateObj.clock.getElapsedTime() * 1.5) % 1
      const scale = 0.5 + t * 1.4
      pulseRingRef.current.scale.set(scale, scale, 1)
      if (pulseRingRef.current.material) {
        pulseRingRef.current.material.opacity = Math.max(0, 0.6 * (1 - t))
      }
    }
  })

  const beaconColor = getStateColor()

  return (
    <group ref={groupRef} position={targetPosition}>
      {/* ── UBER GPS LOCATION RING ON FLOOR ── */}
      <mesh
        ref={pulseRingRef}
        rotation={[-Math.PI / 2, 0, 0]}
        position={[0, -0.18, 0]}
      >
        <ringGeometry args={[0.65, 0.75, 32]} />
        <meshBasicMaterial
          color="#2563eb"
          transparent
          opacity={0.5}
          side={THREE.DoubleSide}
        />
      </mesh>

      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.185, 0]}>
        <circleGeometry args={[0.26, 32]} />
        <meshBasicMaterial
          color="#2563eb"
          transparent
          opacity={0.2}
          side={THREE.DoubleSide}
        />
      </mesh>

      {/* ── MAIN ROVER CHASSIS (Medical-Grade Satin White Body) ── */}
      <mesh castShadow position={[0, 0.15, 0]}>
        <boxGeometry args={[0.7, 0.25, 1.0]} />
        <meshStandardMaterial color="#ffffff" roughness={0.15} metalness={0.05} wireframe={isWireframe} />
      </mesh>

      {/* Charcoal Accent Side Trim Strips */}
      <mesh position={[0.36, 0.15, 0]}>
        <boxGeometry args={[0.02, 0.08, 0.9]} />
        <meshStandardMaterial color="#0284c7" wireframe={isWireframe} />
      </mesh>
      <mesh position={[-0.36, 0.15, 0]}>
        <boxGeometry args={[0.02, 0.08, 0.9]} />
        <meshStandardMaterial color="#0284c7" wireframe={isWireframe} />
      </mesh>

      {/* ── MEDICINE DELIVERY COMPARTMENT ── */}
      <mesh castShadow position={[0, 0.3, 0.05]}>
        <boxGeometry args={[0.55, 0.16, 0.6]} />
        <meshStandardMaterial color="#334155" roughness={0.3} wireframe={isWireframe} />
      </mesh>

      {/* Medicine Box inside */}
      <group position={[0, 0.33, 0.05]}>
        <mesh>
          <boxGeometry args={[0.22, 0.12, 0.26]} />
          <meshStandardMaterial color="#ffffff" wireframe={isWireframe} />
        </mesh>
        {/* Red Cross */}
        <mesh position={[0, 0.065, 0]}>
          <boxGeometry args={[0.12, 0.01, 0.04]} />
          <meshBasicMaterial color="#ef4444" wireframe={isWireframe} />
        </mesh>
        <mesh position={[0, 0.065, 0]}>
          <boxGeometry args={[0.04, 0.01, 0.12]} />
          <meshBasicMaterial color="#ef4444" wireframe={isWireframe} />
        </mesh>
      </group>

      {/* Motorized Glass Lid */}
      <mesh ref={lidRef} position={[0, 0.42, 0.05]}>
        <boxGeometry args={[0.5, 0.04, 0.55]} />
        <meshStandardMaterial
          color="#38bdf8"
          opacity={0.5}
          transparent
          roughness={0.1}
          wireframe={isWireframe}
        />
      </mesh>

      {/* ── LIDAR / SENSOR DOME ── */}
      <group position={[0, 0.45, -0.3]}>
        <mesh>
          <cylinderGeometry args={[0.08, 0.1, 0.06, 16]} />
          <meshStandardMaterial color="#64748b" />
        </mesh>
        <mesh ref={lidarRef} position={[0, 0.05, 0]}>
          <cylinderGeometry args={[0.07, 0.07, 0.08, 16]} />
          <meshStandardMaterial color="#0284c7" />
        </mesh>
      </group>

      {/* ── STATUS BEACON LIGHT ── */}
      <group position={[0, 0.48, 0.3]}>
        <mesh>
          <cylinderGeometry args={[0.04, 0.04, 0.09, 16]} />
          <meshStandardMaterial color={beaconColor} emissive={beaconColor} emissiveIntensity={1.2} />
        </mesh>
        <pointLight color={beaconColor} intensity={1.5} distance={1.8} />
      </group>

      {/* ── FRONT HEADLIGHTS ── */}
      <group position={[0, 0.14, -0.51]}>
        <mesh position={[-0.24, 0, 0]}>
          <boxGeometry args={[0.08, 0.04, 0.02]} />
          <meshStandardMaterial color="#ffffff" emissive="#ffffff" emissiveIntensity={1.5} />
        </mesh>
        <spotLight
          position={[-0.24, 0, 0]}
          target-position={[-0.24, -0.5, -4]}
          color="#ffffff"
          intensity={1.5}
          angle={0.5}
          penumbra={0.6}
          distance={6}
        />

        <mesh position={[0.24, 0, 0]}>
          <boxGeometry args={[0.08, 0.04, 0.02]} />
          <meshStandardMaterial color="#ffffff" emissive="#ffffff" emissiveIntensity={1.5} />
        </mesh>
        <spotLight
          position={[0.24, 0, 0]}
          target-position={[0.24, -0.5, -4]}
          color="#ffffff"
          intensity={1.5}
          angle={0.5}
          penumbra={0.6}
          distance={6}
        />
      </group>

      {/* ── FRONT BUMPER & IR SENSORS ── */}
      <group position={[0, 0.06, -0.52]}>
        <mesh>
          <boxGeometry args={[0.65, 0.06, 0.05]} />
          <meshStandardMaterial color="#334155" />
        </mesh>
        {[-0.25, -0.125, 0, 0.125, 0.25].map((xOffset, i) => (
          <mesh key={i} position={[xOffset, 0, -0.03]}>
            <sphereGeometry args={[0.025, 12, 12]} />
            <meshStandardMaterial color="#dc2626" />
          </mesh>
        ))}
      </group>

      {/* ── 4 WHEELS WITH FRONT STEERING PIVOTS ── */}
      <group ref={(el) => (frontSteerRefs.current[0] = el)} position={[-0.38, 0.08, -0.3]}>
        <Wheel ref={(el) => (wheelRefs.current[0] = el)} position={[0, 0, 0]} />
      </group>

      <group ref={(el) => (frontSteerRefs.current[1] = el)} position={[0.38, 0.08, -0.3]}>
        <Wheel ref={(el) => (wheelRefs.current[1] = el)} position={[0, 0, 0]} />
      </group>

      <Wheel ref={(el) => (wheelRefs.current[2] = el)} position={[-0.38, 0.08, 0.3]} />
      <Wheel ref={(el) => (wheelRefs.current[3] = el)} position={[0.38, 0.08, 0.3]} />

      {/* ── OVERHEAD HUMAN-DESIGNED WHITE PILL BADGE ── */}
      <Html position={[0, 0.8, 0]} center distanceFactor={10}>
        <div className="flex items-center gap-1.5 bg-white/95 text-slate-800 text-xs px-3 py-1 rounded-full border border-slate-200 shadow-md backdrop-blur-md whitespace-nowrap select-none pointer-events-none">
          <span className="w-2 h-2 rounded-full" style={{ backgroundColor: beaconColor }} />
          <span className="font-bold tracking-wide text-slate-900">MedRover #01</span>
          <span className="text-[10px] text-slate-500 font-mono font-medium">
            {speed > 0.05 ? `${speed.toFixed(1)} m/s` : state.toUpperCase()}
          </span>
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
        <meshStandardMaterial color="#334155" roughness={0.8} />
      </mesh>
      {/* Hubcap */}
      <mesh position={[0, 0.041, 0]}>
        <cylinderGeometry args={[0.06, 0.06, 0.002, 16]} />
        <meshStandardMaterial color="#f8fafc" roughness={0.2} metalness={0.2} />
      </mesh>
    </group>
  )
})
