import React, { useRef } from 'react'
import { Text, Float, Line } from '@react-three/drei'
import { useFrame } from '@react-three/fiber'
import * as THREE from 'three'

// Room coordinate topology definition for 3 hospital bays
export const TOPOLOGY = {
  BASE: { x: -6.5, z: 1.5, name: 'Docking Base' },
  // Junctions on the corridor spine (at z = 1.5)
  J_A: { x: -3.4, z: 1.5, name: 'Junction Room A' },
  J_B: { x: 0.0, z: 1.5, name: 'Junction Room B' },
  J_C: { x: 3.4, z: 1.5, name: 'Junction Room C' },
  // Room drop-off zones inside the bays (at z = -1.2)
  ROOM_A: { x: -3.4, z: -1.2, name: 'Room A (General)', code: 'A' },
  ROOM_B: { x: 0.0, z: -1.2, name: 'Room B (ICU)', code: 'B' },
  ROOM_C: { x: 3.4, z: -1.2, name: 'Room C (Pediatric)', code: 'C' }
}

export const ROOMS_CONFIG = [
  { id: 'ROOM_A', code: 'A', name: 'Room A (General)', shortName: 'Room A', signText: 'ROOM A (GENERAL)', subtitle: 'General Ward', x: -3.4, color: '#38bdf8', status: 'ACTIVE' },
  { id: 'ROOM_B', code: 'B', name: 'Room B (ICU)', shortName: 'Room B', signText: 'ROOM B (ICU)', subtitle: 'ICU Ward', x: 0.0, color: '#4ade80', status: 'ACTIVE' },
  { id: 'ROOM_C', code: 'C', name: 'Room C (Pediatric)', shortName: 'Room C', signText: 'ROOM C (PEDIATRIC)', subtitle: 'Pediatric Ward', x: 3.4, color: '#facc15', status: 'ACTIVE' }
]

export function CorridorLayout({ activeRoom, onSelectRoom, activeRoutePoints = [], isWireframe = false }) {
  const pulseRef = useRef()

  useFrame((state) => {
    if (pulseRef.current) {
      const s = 1 + Math.sin(state.clock.getElapsedTime() * 3.5) * 0.12
      pulseRef.current.scale.set(s, s, 1)
    }
  })

  return (
    <group>
      {/* ── GROUND FLOOR (Warm Clean Hospital Floor from Reference Image) ── */}
      <mesh receiveShadow rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.01, -2]}>
        <planeGeometry args={[30, 24]} />
        <meshStandardMaterial color={isWireframe ? '#0f172a' : '#f7f3ec'} roughness={0.4} metalness={0.02} wireframe={isWireframe} />
      </mesh>

      {/* Floor tile grid lines */}
      <gridHelper args={[30, 30, isWireframe ? '#38bdf8' : '#e5ded3', isWireframe ? '#1e293b' : '#ede7dc']} position={[0, 0.001, -2]} />

      {/* ── REAR TABLE / CANOPY STRUCTURE (Dark Architectural Roof Canopy across 3 bays) ── */}
      <group position={[0, 0, -4.5]}>
        {/* Canopy Top Roof Slab */}
        <mesh castShadow receiveShadow position={[0, 2.7, 0]}>
          <boxGeometry args={[11.8, 0.35, 4.8]} />
          <meshStandardMaterial color="#273142" roughness={0.3} metalness={0.1} wireframe={isWireframe} />
        </mesh>
        {/* Support Legs */}
        <mesh position={[-5.6, 1.35, -2.2]} castShadow>
          <cylinderGeometry args={[0.18, 0.18, 2.7, 16]} />
          <meshStandardMaterial color="#1e293b" wireframe={isWireframe} />
        </mesh>
        <mesh position={[-5.6, 1.35, 2.2]} castShadow>
          <cylinderGeometry args={[0.18, 0.18, 2.7, 16]} />
          <meshStandardMaterial color="#1e293b" wireframe={isWireframe} />
        </mesh>
        <mesh position={[5.6, 1.35, -2.2]} castShadow>
          <cylinderGeometry args={[0.18, 0.18, 2.7, 16]} />
          <meshStandardMaterial color="#1e293b" wireframe={isWireframe} />
        </mesh>
        <mesh position={[5.6, 1.35, 2.2]} castShadow>
          <cylinderGeometry args={[0.18, 0.18, 2.7, 16]} />
          <meshStandardMaterial color="#1e293b" wireframe={isWireframe} />
        </mesh>
      </group>

      {/* ── 3 HOSPITAL BAYS SIDE-BY-SIDE (Room A, Room B, Room C) ── */}
      {ROOMS_CONFIG.map((room) => {
        const isSelected = activeRoom === room.code || activeRoom === room.id
        return (
          <HospitalCubicle
            key={room.id}
            room={room}
            isSelected={isSelected}
            isWireframe={isWireframe}
            onSelect={() => onSelectRoom && onSelectRoom(room.code)}
            pulseRef={isSelected ? pulseRef : null}
          />
        )
      })}

      {/* ── CORRIDOR FLOOR GUIDANCE LINE (From Base Dock across 3 bays) ── */}
      {/* Central horizontal corridor track at z = 1.5 */}
      <mesh position={[-0.8, 0.004, 1.5]}>
        <boxGeometry args={[12.2, 0.002, 0.22]} />
        <meshStandardMaterial color="#475569" roughness={0.6} />
      </mesh>
      <mesh position={[-0.8, 0.005, 1.5]}>
        <boxGeometry args={[12.2, 0.002, 0.06]} />
        <meshStandardMaterial color="#0284c7" />
      </mesh>

      {/* Spur tracks turning into each room */}
      {ROOMS_CONFIG.map((room) => (
        <group key={`track-${room.id}`}>
          {/* Spur track into bay */}
          <mesh position={[room.x, 0.004, 0.15]}>
            <boxGeometry args={[0.22, 0.002, 2.7]} />
            <meshStandardMaterial color="#475569" roughness={0.6} />
          </mesh>
          <mesh position={[room.x, 0.005, 0.15]}>
            <boxGeometry args={[0.06, 0.002, 2.7]} />
            <meshStandardMaterial color="#0284c7" />
          </mesh>
          {/* Junction dot */}
          <mesh position={[room.x, 0.008, 1.5]} rotation={[-Math.PI / 2, 0, 0]}>
            <ringGeometry args={[0.2, 0.28, 20]} />
            <meshStandardMaterial color="#64748b" />
          </mesh>
        </group>
      ))}

      {/* ── BASE CHARGING DOCK PAD ── */}
      <group position={[TOPOLOGY.BASE.x, 0.006, TOPOLOGY.BASE.z]}>
        <mesh rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[0.65, 0.78, 32]} />
          <meshStandardMaterial color="#10b981" />
        </mesh>
        <mesh rotation={[-Math.PI / 2, 0, 0]}>
          <circleGeometry args={[0.55, 32]} />
          <meshStandardMaterial color="#d1fae5" />
        </mesh>
        <Text
          position={[0, 0.02, 0.95]}
          rotation={[-Math.PI / 2, 0, 0]}
          fontSize={0.24}
          color="#065f46"
          fontWeight="bold"
          anchorX="center"
        >
          DOCK / BASE
        </Text>
      </group>

      {/* ── UBER-STYLE ACTIVE ROUTE GLOWING POLYLINE ── */}
      {activeRoutePoints.length >= 2 && (
        <group position={[0, 0.015, 0]}>
          <Line
            points={activeRoutePoints}
            color="#2563eb"
            lineWidth={4.5}
          />
          <Line
            points={activeRoutePoints}
            color="#93c5fd"
            lineWidth={9}
            transparent
            opacity={0.4}
          />
        </group>
      )}
    </group>
  )
}

/**
 * Individual Hospital Bay Cubicle matching reference image
 */
function HospitalCubicle({ room, isSelected, isWireframe = false, onSelect, pulseRef }) {
  const { x, name, color, status } = room

  return (
    <group position={[x, 0, -2.4]} onClick={(e) => { e.stopPropagation(); onSelect(); }}>
      {/* ── Cubicle Floor Pad ── */}
      <mesh position={[0, 0.003, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[2.9, 3.2]} />
        <meshStandardMaterial color={isSelected ? '#ffffff' : '#f8fafc'} roughness={0.3} wireframe={isWireframe} />
      </mesh>

      {/* Target Drop Zone Ring on Floor (Exact Match to Screenshot 1) */}
      <mesh ref={pulseRef} position={[0, 0.008, 1.3]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.54, 0.72, 36]} />
        <meshStandardMaterial color={color} wireframe={isWireframe} roughness={0.2} />
      </mesh>

      {/* ── Fluted / Slatted Vertical Side Wall Dividers (Left & Right) ── */}
      {/* Left Wall Fluted Slats */}
      <FlutedWall position={[-1.45, 1.1, 0]} isWireframe={isWireframe} />
      {/* Right Wall Fluted Slats */}
      <FlutedWall position={[1.45, 1.1, 0]} isWireframe={isWireframe} />

      {/* Rear Wall with Horizontal Accent Lines */}
      <mesh position={[0, 1.1, -1.55]}>
        <boxGeometry args={[2.85, 2.2, 0.08]} />
        <meshStandardMaterial color="#334155" roughness={0.4} wireframe={isWireframe} />
      </mesh>
      {/* White horizontal trim stripes on back wall */}
      <mesh position={[0, 1.35, -1.5]}>
        <boxGeometry args={[2.6, 0.04, 0.02]} />
        <meshStandardMaterial color="#f1f5f9" wireframe={isWireframe} />
      </mesh>

      {/* Cubicle Ceiling Canopy Slab */}
      <mesh position={[0, 2.25, 0]}>
        <boxGeometry args={[3.0, 0.12, 3.3]} />
        <meshStandardMaterial color="#e2e8f0" roughness={0.3} wireframe={isWireframe} />
      </mesh>
      {/* Front ceiling fascia colored to room */}
      <mesh position={[0, 2.3, 1.62]}>
        <boxGeometry args={[3.0, 0.16, 0.08]} />
        <meshStandardMaterial color={color} wireframe={isWireframe} roughness={0.2} />
      </mesh>
      <mesh position={[0, 2.21, 1.63]}>
        <boxGeometry args={[3.0, 0.03, 0.09]} />
        <meshStandardMaterial color="#ffffff" wireframe={isWireframe} />
      </mesh>

      {/* Interior Ceiling Light Bar */}
      <mesh position={[0, 2.18, 0]}>
        <boxGeometry args={[1.6, 0.03, 0.1]} />
        <meshStandardMaterial color="#ffffff" emissive="#ffffff" emissiveIntensity={isWireframe ? 0.2 : 1.2} wireframe={isWireframe} />
      </mesh>
      <pointLight position={[0, 2.0, 0]} intensity={isSelected ? 1.2 : 0.6} color="#ffffff" distance={4} />

      {/* ── Hospital Patient Bed Furniture ── */}
      <group position={[-0.4, 0, 0.2]}>
        {/* Bed Frame */}
        <mesh position={[0, 0.22, 0]}>
          <boxGeometry args={[1.1, 0.25, 1.9]} />
          <meshStandardMaterial color="#475569" roughness={0.3} wireframe={isWireframe} />
        </mesh>
        {/* Mattress with room accent sheet */}
        <mesh position={[0, 0.38, 0]}>
          <boxGeometry args={[1.02, 0.12, 1.82]} />
          <meshStandardMaterial color="#ffffff" roughness={0.6} wireframe={isWireframe} />
        </mesh>
        {/* Sheet / Blanket colored */}
        <mesh position={[0, 0.39, 0.25]}>
          <boxGeometry args={[1.03, 0.11, 1.2]} />
          <meshStandardMaterial color={color} roughness={0.5} wireframe={isWireframe} />
        </mesh>
        {/* White Pillow */}
        <mesh position={[0, 0.46, -0.65]}>
          <boxGeometry args={[0.7, 0.08, 0.35]} />
          <meshStandardMaterial color="#f8fafc" wireframe={isWireframe} />
        </mesh>
        {/* Headboard */}
        <mesh position={[0, 0.55, -0.96]}>
          <boxGeometry args={[1.1, 0.5, 0.06]} />
          <meshStandardMaterial color="#64748b" wireframe={isWireframe} />
        </mesh>
      </group>

      {/* ── Medical Patient Monitor on Stand ── */}
      <group position={[0.75, 0, 0.5]}>
        {/* Stand Base */}
        <mesh position={[0, 0.03, 0]}>
          <cylinderGeometry args={[0.18, 0.18, 0.06, 16]} />
          <meshStandardMaterial color="#1e293b" wireframe={isWireframe} />
        </mesh>
        {/* Pole */}
        <mesh position={[0, 0.6, 0]}>
          <cylinderGeometry args={[0.025, 0.025, 1.15, 16]} />
          <meshStandardMaterial color="#94a3b8" metalness={0.6} wireframe={isWireframe} />
        </mesh>
        {/* Monitor Screen Case */}
        <mesh position={[0, 1.15, 0]}>
          <boxGeometry args={[0.42, 0.3, 0.05]} />
          <meshStandardMaterial color="#0f172a" wireframe={isWireframe} />
        </mesh>
        {/* Monitor Display */}
        <mesh position={[0, 1.15, 0.027]}>
          <planeGeometry args={[0.38, 0.26]} />
          <meshStandardMaterial color="#0284c7" emissive="#0284c7" emissiveIntensity={isWireframe ? 0.2 : 0.8} wireframe={isWireframe} />
        </mesh>
      </group>

      {/* ── Medical Wall Cabinet (on right side wall) ── */}
      <group position={[1.35, 1.4, 0.1]}>
        <mesh>
          <boxGeometry args={[0.14, 0.65, 0.35]} />
          <meshStandardMaterial color={color} wireframe={isWireframe} roughness={0.3} />
        </mesh>
      </group>

      {/* ── 3D SIGNBOARD ABOVE CUBICLE (Exact Match to Screenshot 1) ── */}
      <group position={[0, 2.72, 1.5]}>
        {/* Outer Sign Frame */}
        <mesh position={[0, 0, 0]}>
          <boxGeometry args={[2.5, 0.58, 0.08]} />
          <meshStandardMaterial color="#0b1329" roughness={0.3} wireframe={isWireframe} />
        </mesh>
        <mesh position={[0, 0, 0.041]}>
          <boxGeometry args={[2.44, 0.52, 0.01]} />
          <meshStandardMaterial color="#0f172a" wireframe={isWireframe} />
        </mesh>

        {/* Room Name Text (e.g., ROOM A (GENERAL), ROOM B (ICU), ROOM C (PEDIATRIC)) */}
        <Text
          position={[-0.48, 0, 0.05]}
          fontSize={0.21}
          color="#ffffff"
          fontWeight="bold"
          anchorX="center"
          anchorY="middle"
        >
          {room.signText || name}
        </Text>

        {/* Status Pill Badge (e.g., ● ACTIVE) */}
        <group position={[0.55, 0, 0.05]}>
          <mesh>
            <boxGeometry args={[0.9, 0.36, 0.02]} />
            <meshStandardMaterial
              color={isSelected ? '#064e3b' : '#052e16'}
              wireframe={isWireframe}
            />
          </mesh>
          <mesh position={[-0.28, 0, 0.02]}>
            <circleGeometry args={[0.04, 16]} />
            <meshBasicMaterial color="#22c55e" />
          </mesh>
          <Text
            position={[0.06, 0, 0.03]}
            fontSize={0.15}
            color="#4ade80"
            fontWeight="bold"
            anchorX="center"
            anchorY="middle"
          >
            {isSelected ? 'DELIVERING' : status}
          </Text>
        </group>
      </group>
    </group>
  )
}

/**
 * Modern Architectural Fluted / Slatted Wall
 */
function FlutedWall({ position, isWireframe = false }) {
  return (
    <group position={position}>
      {/* Base divider core */}
      <mesh>
        <boxGeometry args={[0.08, 2.2, 3.2]} />
        <meshStandardMaterial color="#1e293b" roughness={0.4} wireframe={isWireframe} />
      </mesh>
      {/* Vertical slats */}
      {[-1.3, -0.9, -0.5, -0.1, 0.3, 0.7, 1.1].map((zOffset, i) => (
        <mesh key={i} position={[0, 0, zOffset]}>
          <boxGeometry args={[0.12, 2.2, 0.08]} />
          <meshStandardMaterial color="#334155" roughness={0.3} wireframe={isWireframe} />
        </mesh>
      ))}
    </group>
  )
}
