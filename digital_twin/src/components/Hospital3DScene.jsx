import React, { useRef, useEffect } from 'react'
import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera, ContactShadows } from '@react-three/drei'
import * as THREE from 'three'
import { CorridorLayout } from './CorridorLayout'
import { RoverModel } from './RoverModel'

function CameraController({ cameraMode, roverPosition }) {
  const { camera } = useThree()
  const controlsRef = useRef()

  useEffect(() => {
    if (!controlsRef.current) return

    if (cameraMode === 'topdown') {
      // Top down aerial overview
      camera.position.set(-2, 18, -7)
      controlsRef.current.target.set(-2, 0, -7)
    } else if (cameraMode === 'follow') {
      // Third person follow view behind rover
      camera.position.set(roverPosition[0], roverPosition[1] + 3, roverPosition[2] + 5)
      controlsRef.current.target.set(roverPosition[0], roverPosition[1], roverPosition[2])
    } else if (cameraMode === 'free') {
      // Default isometric view
      camera.position.set(7, 10, 6)
      controlsRef.current.target.set(-2, 0, -7)
    }
    controlsRef.current.update()
  }, [cameraMode])

  useFrame((_, delta) => {
    if (cameraMode === 'follow' && controlsRef.current) {
      // Continuously update follow target as rover moves
      const targetCamX = THREE.MathUtils.lerp(camera.position.x, roverPosition[0], delta * 3)
      const targetCamZ = THREE.MathUtils.lerp(camera.position.z, roverPosition[2] + 4, delta * 3)
      camera.position.x = targetCamX
      camera.position.z = targetCamZ
      controlsRef.current.target.set(roverPosition[0], roverPosition[1], roverPosition[2])
      controlsRef.current.update()
    }
  })

  return (
    <OrbitControls
      ref={controlsRef}
      makeDefault
      maxPolarAngle={Math.PI / 2 - 0.05} // Prevent camera going under floor
      minDistance={2}
      maxDistance={35}
    />
  )
}

export function Hospital3DScene({ roverState, cameraMode = 'free' }) {
  const { position, yaw, state, activeRoom, battery } = roverState

  return (
    <div className="w-full h-full relative bg-slate-950">
      <Canvas shadows gl={{ antialias: true, alpha: false }}>
        <color attach="background" args={['#090d16']} />
        <fog attach="fog" args={['#090d16', 15, 35]} />

        <PerspectiveCamera makeDefault fov={50} position={[7, 10, 6]} />
        <CameraController cameraMode={cameraMode} roverPosition={position} />

        {/* ── LIGHTING SETUP ── */}
        <ambientLight intensity={0.6} />
        <directionalLight
          position={[10, 20, 10]}
          intensity={1.2}
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={40}
          shadow-camera-left={-15}
          shadow-camera-right={15}
          shadow-camera-top={15}
          shadow-camera-bottom={-15}
        />
        {/* Soft Ambient Fill Blue Light */}
        <pointLight position={[-10, 10, -10]} intensity={0.8} color="#0284c7" />
        <pointLight position={[5, 8, -5]} intensity={0.5} color="#38bdf8" />

        {/* ── HOSPITAL SCENE & TRACK Topography ── */}
        <CorridorLayout activeRoom={activeRoom} />

        {/* ── ROVER 3D MODEL ── */}
        <RoverModel
          targetPosition={position}
          yaw={yaw}
          state={state}
          battery={battery}
        />

        {/* ── CONTACT SHADOW ── */}
        <ContactShadows position={[0, 0.01, 0]} opacity={0.7} scale={20} blur={1.5} far={4} />
      </Canvas>
    </div>
  )
}
