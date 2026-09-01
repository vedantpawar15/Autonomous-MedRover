import React from 'react'
import { Text, Float, Html } from '@react-three/drei'

// Room coordinate topology definition
export const TOPOLOGY = {
  BASE: { x: 0, z: 0, name: 'Docking Base' },
  J1: { x: 0, z: -4, name: 'Junction 1 (Room A)' },
  J2: { x: 0, z: -8, name: 'Junction 2 (Room B)' },
  J3: { x: 0, z: -12, name: 'Junction 3 (Room C)' },
  ROOM_A: { x: -5, z: -4, name: 'Room A (General)' },
  ROOM_B: { x: -5, z: -8, name: 'Room B (ICU)' },
  ROOM_C: { x: -5, z: -12, name: 'Room C (Pediatric)' }
}

export function CorridorLayout({ activeRoom }) {
  return (
    <group>
      {/* ── Main Hospital Floor Tile ── */}
      <mesh receiveShadow rotation={[-Math.PI / 2, 0, 0]} position={[-2, -0.01, -7]}>
        <planeGeometry args={[18, 22]} />
        <meshStandardMaterial color="#0f172a" roughness={0.4} metalness={0.1} />
      </mesh>

      {/* ── Floor Tile Grid Accent Lines ── */}
      <gridHelper args={[24, 24, '#1e293b', '#1e293b']} position={[-2, 0.001, -7]} />

      {/* ── FLOOR TAPE GUIDETRACK (Black & Glowing Cyan Guidance Line) ── */}
      {/* Central Corridor Spine Line */}
      <mesh position={[0, 0.005, -7]}>
        <boxGeometry args={[0.2, 0.002, 14]} />
        <meshStandardMaterial color="#00f0ff" emissive="#00f0ff" emissiveIntensity={0.6} />
      </mesh>

      {/* Base Docking Pad */}
      <group position={[0, 0.006, 0]}>
        <mesh>
          <ringGeometry args={[0.6, 0.8, 32]} rotation={[-Math.PI / 2, 0, 0]} />
          <meshStandardMaterial color="#10b981" emissive="#10b981" emissiveIntensity={0.8} />
        </mesh>
        <mesh position={[0, 0, 0]}>
          <circleGeometry args={[0.5, 32]} rotation={[-Math.PI / 2, 0, 0]} />
          <meshStandardMaterial color="#064e3b" opacity={0.6} transparent />
        </mesh>
        <Text position={[0, 0.02, 0.9]} rotation={[-Math.PI / 2, 0, 0]} fontSize={0.3} color="#10b981" anchorX="center">
          DOCK / BASE
        </Text>
      </group>

      {/* ── SPURS TO ROOMS A, B, C ── */}
      {/* Spur 1 to Room A */}
      <mesh position={[-2.5, 0.005, -4]}>
        <boxGeometry args={[5, 0.002, 0.2]} />
        <meshStandardMaterial color={activeRoom === 'A' ? '#3b82f6' : '#00f0ff'} emissive={activeRoom === 'A' ? '#3b82f6' : '#00f0ff'} emissiveIntensity={0.6} />
      </mesh>

      {/* Spur 2 to Room B */}
      <mesh position={[-2.5, 0.005, -8]}>
        <boxGeometry args={[5, 0.002, 0.2]} />
        <meshStandardMaterial color={activeRoom === 'B' ? '#3b82f6' : '#00f0ff'} emissive={activeRoom === 'B' ? '#3b82f6' : '#00f0ff'} emissiveIntensity={0.6} />
      </mesh>

      {/* Spur 3 to Room C */}
      <mesh position={[-2.5, 0.005, -12]}>
        <boxGeometry args={[5, 0.002, 0.2]} />
        <meshStandardMaterial color={activeRoom === 'C' ? '#3b82f6' : '#00f0ff'} emissive={activeRoom === 'C' ? '#3b82f6' : '#00f0ff'} emissiveIntensity={0.6} />
      </mesh>

      {/* ── JUNCTION NODE MARKERS ── */}
      {['J1', 'J2', 'J3'].map((jKey, idx) => {
        const j = TOPOLOGY[jKey]
        return (
          <group key={jKey} position={[j.x, 0.01, j.z]}>
            <mesh rotation={[-Math.PI / 2, 0, 0]}>
              <ringGeometry args={[0.3, 0.4, 16]} />
              <meshStandardMaterial color="#f59e0b" emissive="#f59e0b" emissiveIntensity={0.9} />
            </mesh>
            <Text position={[0.6, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]} fontSize={0.25} color="#f59e0b" anchorX="left">
              {jKey}
            </Text>
          </group>
        )
      })}

      {/* ── ROOM ENCLOSURES & LABELS ── */}
      {/* ROOM A */}
      <RoomStructure
        position={[-6, 0, -4]}
        label="ROOM A"
        subtitle="General Ward"
        color="#3b82f6"
        isActive={activeRoom === 'A'}
      />

      {/* ROOM B */}
      <RoomStructure
        position={[-6, 0, -8]}
        label="ROOM B"
        subtitle="ICU Ward"
        color="#ec4899"
        isActive={activeRoom === 'B'}
      />

      {/* ROOM C */}
      <RoomStructure
        position={[-6, 0, -12]}
        label="ROOM C"
        subtitle="Pediatric Ward"
        color="#8b5cf6"
        isActive={activeRoom === 'C'}
      />

      {/* ── CORRIDOR WALL ACCENTS (Right side) ── */}
      <mesh position={[3, 1, -7]}>
        <boxGeometry args={[0.2, 2, 18]} />
        <meshStandardMaterial color="#1e293b" opacity={0.4} transparent />
      </mesh>
      <mesh position={[3, 2.05, -7]}>
        <boxGeometry args={[0.25, 0.1, 18]} />
        <meshStandardMaterial color="#00f0ff" emissive="#00f0ff" emissiveIntensity={0.5} />
      </mesh>
    </group>
  )
}

function RoomStructure({ position, label, subtitle, color, isActive }) {
  return (
    <group position={position}>
      {/* Room Floor Pad */}
      <mesh position={[0, 0.002, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[3.2, 3.2]} />
        <meshStandardMaterial color={isActive ? color : '#1e293b'} opacity={0.8} transparent />
      </mesh>

      {/* Target Drop Zone Ring */}
      <mesh position={[0, 0.008, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.6, 0.75, 32]} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={isActive ? 1.5 : 0.5} />
      </mesh>

      {/* Glass Walls */}
      {/* Back Wall */}
      <mesh position={[-1.6, 0.75, 0]}>
        <boxGeometry args={[0.1, 1.5, 3.2]} />
        <meshStandardMaterial color={color} opacity={0.25} transparent roughness={0.1} />
      </mesh>
      {/* Side Wall North */}
      <mesh position={[0, 0.75, -1.6]}>
        <boxGeometry args={[3.2, 1.5, 0.1]} />
        <meshStandardMaterial color={color} opacity={0.25} transparent roughness={0.1} />
      </mesh>
      {/* Side Wall South */}
      <mesh position={[0, 0.75, 1.6]}>
        <boxGeometry args={[3.2, 1.5, 0.1]} />
        <meshStandardMaterial color={color} opacity={0.25} transparent roughness={0.1} />
      </mesh>

      {/* Hospital Bed Placeholder inside room */}
      <mesh position={[-0.6, 0.3, 0]}>
        <boxGeometry args={[1.4, 0.4, 0.9]} />
        <meshStandardMaterial color="#334155" />
      </mesh>
      <mesh position={[-0.6, 0.55, -0.2]}>
        <boxGeometry args={[1.3, 0.1, 0.7]} />
        <meshStandardMaterial color="#e2e8f0" />
      </mesh>

      {/* Glowing Floating 3D Room Title Banner */}
      <Float speed={2} rotationIntensity={0.1} floatIntensity={0.2}>
        <Text
          position={[0, 2.2, 0]}
          fontSize={0.4}
          color={isActive ? '#ffffff' : color}
          anchorX="center"
          anchorY="middle"
          fontWeight="bold"
        >
          {label}
        </Text>
        <Text
          position={[0, 1.8, 0]}
          fontSize={0.2}
          color="#94a3b8"
          anchorX="center"
          anchorY="middle"
        >
          {subtitle}
        </Text>
      </Float>

      {isActive && (
        <pointLight position={[0, 1.5, 0]} color={color} intensity={2} distance={5} />
      )}
    </group>
  )
}
