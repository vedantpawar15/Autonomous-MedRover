import React, { useRef, useEffect, useState } from 'react'
import { Canvas, useFrame, useThree } from '@react-three/fiber'
import { OrbitControls, PerspectiveCamera, ContactShadows, GizmoHelper, GizmoViewport, Text } from '@react-three/drei'
import * as THREE from 'three'
import { CorridorLayout, ROOMS_CONFIG } from './CorridorLayout'
import { RoverModel } from './RoverModel'

// Default camera pose framing 3 rooms (Room A, Room B, Room C) centered at x=0
const DEFAULT_CAM_POS = [0.0, 2.5, 4.4]
const DEFAULT_CAM_TARGET = [0.0, 1.4, -2.4]

/**
 * High-performance 3D Game + CAD Flight & Walk Controller
 * Supports WASD + Arrow Keys + Space/Shift + Mouse Look & Orbit
 */
function GameCameraController({
  cameraMode,
  roverPosition,
  focusRoom,
  resetTrigger,
  controlsRef,
  onKeysChange
}) {
  const { camera } = useThree()
  const keys = useRef({
    forward: false,
    backward: false,
    left: false,
    right: false,
    up: false,
    down: false,
    shift: false
  })

  const isTransitioningRef = useRef(false)
  const targetCamPosRef = useRef(new THREE.Vector3(...DEFAULT_CAM_POS))
  const targetLookAtRef = useRef(new THREE.Vector3(...DEFAULT_CAM_TARGET))

  // ── KEYBOARD WASD & ARROW KEYS EVENT HANDLERS ──
  useEffect(() => {
    const notifyKeys = () => {
      onKeysChange?.({
        w: keys.current.forward,
        s: keys.current.backward,
        a: keys.current.left,
        d: keys.current.right,
        shift: keys.current.shift,
        space: keys.current.up
      })
    }

    const onKeyDown = (e) => {
      if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Space'].includes(e.code)) {
        e.preventDefault()
      }

      let changed = false
      if (e.code === 'KeyW' || e.code === 'ArrowUp') { keys.current.forward = true; changed = true; }
      if (e.code === 'KeyS' || e.code === 'ArrowDown') { keys.current.backward = true; changed = true; }
      if (e.code === 'KeyA' || e.code === 'ArrowLeft') { keys.current.left = true; changed = true; }
      if (e.code === 'KeyD' || e.code === 'ArrowRight') { keys.current.right = true; changed = true; }
      if (e.code === 'KeyE' || e.code === 'Space') { keys.current.up = true; changed = true; }
      if (e.code === 'KeyQ' || e.code === 'KeyC') { keys.current.down = true; changed = true; }
      if (e.shiftKey) { keys.current.shift = true; changed = true; }

      if (changed) notifyKeys()
    }

    const onKeyUp = (e) => {
      let changed = false
      if (e.code === 'KeyW' || e.code === 'ArrowUp') { keys.current.forward = false; changed = true; }
      if (e.code === 'KeyS' || e.code === 'ArrowDown') { keys.current.backward = false; changed = true; }
      if (e.code === 'KeyA' || e.code === 'ArrowLeft') { keys.current.left = false; changed = true; }
      if (e.code === 'KeyD' || e.code === 'ArrowRight') { keys.current.right = false; changed = true; }
      if (e.code === 'KeyE' || e.code === 'Space') { keys.current.up = false; changed = true; }
      if (e.code === 'KeyQ' || e.code === 'KeyC') { keys.current.down = false; changed = true; }
      if (!e.shiftKey) { keys.current.shift = false; changed = true; }

      if (changed) notifyKeys()
    }

    window.addEventListener('keydown', onKeyDown, { passive: false })
    window.addEventListener('keyup', onKeyUp)
    return () => {
      window.removeEventListener('keydown', onKeyDown)
      window.removeEventListener('keyup', onKeyUp)
    }
  }, [onKeysChange])

  // ── RESET VIEW TRIGGER ──
  useEffect(() => {
    if (resetTrigger > 0) {
      targetCamPosRef.current.set(...DEFAULT_CAM_POS)
      targetLookAtRef.current.set(...DEFAULT_CAM_TARGET)
      isTransitioningRef.current = true
    }
  }, [resetTrigger])

  // ── FOCUS ON ROOM TRIGGER: Camera does NOT zoom into room; Rover travels to it instead ──
  useEffect(() => {
    // Camera zoom disabled as requested: clicking a room dispatches the rover to that room
  }, [focusRoom])

  // ── CAMERA MODE TRANSITIONS ──
  useEffect(() => {
    if (cameraMode === 'walk') {
      targetCamPosRef.current.set(...WALK_CAM_POS)
      targetLookAtRef.current.set(...WALK_CAM_TARGET)
      isTransitioningRef.current = true
    } else if (cameraMode === 'free') {
      targetCamPosRef.current.set(...DEFAULT_CAM_POS)
      targetLookAtRef.current.set(...DEFAULT_CAM_TARGET)
      isTransitioningRef.current = true
    }
  }, [cameraMode])

  // ── 60 FPS 3D GAME MOTION & SMOOTH GLIDE LOOP ──
  useFrame((_, delta) => {
    const controls = controlsRef.current
    if (!controls) return

    // Smooth camera glide transition on Reset View or Room Selection
    if (isTransitioningRef.current) {
      camera.position.lerp(targetCamPosRef.current, delta * 4.5)
      controls.target.lerp(targetLookAtRef.current, delta * 4.5)
      controls.update()
      if (camera.position.distanceTo(targetCamPosRef.current) < 0.05) {
        isTransitioningRef.current = false
      }
      return
    }

    // Follow rover mode (chase cam)
    if (cameraMode === 'follow') {
      const targetCamX = THREE.MathUtils.lerp(camera.position.x, roverPosition[0] - 2.8, delta * 4)
      const targetCamY = THREE.MathUtils.lerp(camera.position.y, 3.2, delta * 4)
      const targetCamZ = THREE.MathUtils.lerp(camera.position.z, roverPosition[2] + 4.2, delta * 4)
      camera.position.set(targetCamX, targetCamY, targetCamZ)
      controls.target.set(roverPosition[0], roverPosition[1] + 0.5, roverPosition[2])
      controls.update()
      return
    }

    // Topdown map mode (2D CAD Blueprint)
    if (cameraMode === 'topdown') {
      camera.position.lerp(new THREE.Vector3(0, 19, 0), delta * 4)
      controls.target.lerp(new THREE.Vector3(0, 0, -1), delta * 4)
      controls.update()
      return
    }

    // ── 3D GAME KEYBOARD FLIGHT & WALK MOTION (WASD & ARROWS) ──
    const isMoving =
      keys.current.forward ||
      keys.current.backward ||
      keys.current.left ||
      keys.current.right ||
      keys.current.up ||
      keys.current.down

    if (isMoving) {
      const moveSpeed = keys.current.shift ? 16.0 : 8.5 // Shift = 2x Sprint speed

      // Compute horizontal walk direction from camera facing angle
      const forward = new THREE.Vector3()
      camera.getWorldDirection(forward)

      // Project forward vector horizontally onto floor plane (XZ)
      const walkDir = new THREE.Vector3(forward.x, 0, forward.z)
      if (walkDir.lengthSq() > 0.0001) {
        walkDir.normalize()
      } else {
        walkDir.set(0, 0, -1)
      }

      // Strafe vector perpendicular to walkDir
      const rightDir = new THREE.Vector3(-walkDir.z, 0, walkDir.x)

      const moveVec = new THREE.Vector3()
      if (keys.current.forward) moveVec.add(walkDir)
      if (keys.current.backward) moveVec.sub(walkDir)
      if (keys.current.right) moveVec.add(rightDir)
      if (keys.current.left) moveVec.sub(rightDir)
      if (keys.current.up) moveVec.y += 1.0
      if (keys.current.down) moveVec.y -= 1.0

      if (moveVec.lengthSq() > 0) {
        moveVec.normalize().multiplyScalar(moveSpeed * delta)
        // Translate both camera and orbit target so user glides forward like in a 3D game
        camera.position.add(moveVec)
        controls.target.add(moveVec)

        // Enforce minimum floor clearance so camera doesn't clip under ground
        if (camera.position.y < 0.45) camera.position.y = 0.45

        controls.update()
      }
    }
  })

  return null
}

/**
 * 3D CAD Reticle / Crosshair Indicator Under Mouse Cursor
 */
function CadCursorMarker({ position, active = true }) {
  if (!active || !position) return null

  return (
    <group position={[position.x, 0.012, position.z]}>
      {/* Outer CAD Crosshair Ring */}
      <mesh rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.22, 0.25, 32]} />
        <meshBasicMaterial color="#0284c7" />
      </mesh>
      {/* Inner CAD Target Dot */}
      <mesh rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[0.04, 16]} />
        <meshBasicMaterial color="#38bdf8" />
      </mesh>
      {/* Crosshair Metric Arms */}
      <mesh position={[0, 0.001, 0]}>
        <boxGeometry args={[0.65, 0.002, 0.02]} />
        <meshBasicMaterial color="#0284c7" />
      </mesh>
      <mesh position={[0, 0.001, 0]}>
        <boxGeometry args={[0.02, 0.002, 0.65]} />
        <meshBasicMaterial color="#0284c7" />
      </mesh>
      {/* Live Metric Coordinate Label on Ground */}
      <Text
        position={[0.36, 0.02, 0.36]}
        rotation={[-Math.PI / 2, 0, 0]}
        fontSize={0.15}
        color="#0369a1"
        fontWeight="bold"
      >
        {`X: ${position.x.toFixed(2)}m  Z: ${position.z.toFixed(2)}m`}
      </Text>
    </group>
  )
}

/**
 * CAD 3D Axes & Dimension Blueprint Overlay in Scene
 */
function CadBlueprintGuides() {
  return (
    <group position={[0, 0.002, 0]}>
      {/* ── 3D World Origin CAD Axes Indicator (Red = X, Green = Y, Blue = Z) ── */}
      <axesHelper args={[2.5]} position={[-8.0, 0.02, 1.5]} />

      {/* Origin Axis Labels */}
      <Text
        position={[-5.3, 0.1, 1.5]}
        rotation={[-Math.PI / 2, 0, 0]}
        fontSize={0.22}
        color="#ef4444"
        fontWeight="bold"
      >
        +X LATERAL
      </Text>
      <Text
        position={[-8.0, 0.1, 4.2]}
        rotation={[-Math.PI / 2, 0, 0]}
        fontSize={0.22}
        color="#3b82f6"
        fontWeight="bold"
      >
        +Z LONGITUDINAL
      </Text>

      {/* ── CAD Blueprint Dimension Annotation Labels on Floor ── */}
      <group position={[0, 0.015, 2.3]} rotation={[-Math.PI / 2, 0, 0]}>
        {/* Metric ruler ticks along corridor */}
        {[-8, -5, -1.6, 0, 1.8, 5.2, 8].map((xVal) => (
          <group key={`tick-${xVal}`} position={[xVal, 0, 0]}>
            <mesh position={[0, 0, 0]}>
              <boxGeometry args={[0.04, 0.35, 0.002]} />
              <meshBasicMaterial color="#64748b" />
            </mesh>
            <Text
              position={[0, -0.32, 0]}
              fontSize={0.16}
              color="#64748b"
              fontWeight="bold"
              anchorX="center"
            >
              {`${xVal > 0 ? '+' : ''}${xVal}m`}
            </Text>
          </group>
        ))}

        {/* Main corridor span dimension line */}
        <mesh position={[0, 0.18, 0]}>
          <boxGeometry args={[16.2, 0.02, 0.002]} />
          <meshBasicMaterial color="#94a3b8" />
        </mesh>
        <Text
          position={[0, 0.36, 0]}
          fontSize={0.2}
          color="#0284c7"
          fontWeight="bold"
          anchorX="center"
        >
          ◄────── 16.00 m CENTRAL CLINICAL CORRIDOR ──────►
        </Text>
      </group>
    </group>
  )
}

export function Hospital3DScene({
  roverState,
  activeRoutePoints = [],
  cameraMode = 'free',
  focusRoom = null,
  resetTrigger = 0,
  onSelectRoom,
  isWireframe = false,
  onCadCursorMove,
  onKeysChange
}) {
  const { position, yaw, steeringAngle, isLidOpen, state, activeRoom, battery, speed } = roverState
  const controlsRef = useRef()
  const [cursorPos, setCursorPos] = useState({ x: 0, z: 1.5 })

  const handlePointerMove = (e) => {
    e.stopPropagation()
    const pt = e.point
    setCursorPos({ x: pt.x, z: pt.z })
    onCadCursorMove?.({ x: pt.x, z: pt.z })
  }

  return (
    <div className="w-full h-full relative bg-[#d8e8fa]">
      <Canvas shadows gl={{ antialias: true, alpha: false, preserveDrawingBuffer: true }}>
        {/* Soft, clean CAD drafting sky background */}
        <color attach="background" args={[isWireframe ? '#090d16' : '#d8e8fa']} />
        <fog attach="fog" args={[isWireframe ? '#090d16' : '#d8e8fa', 30, 75]} />

        <PerspectiveCamera makeDefault fov={45} position={DEFAULT_CAM_POS} />

        {/* ── HIGH PRECISION CAD ORBIT CONTROLS (MOUSE NAVIGATION) ── */}
        <OrbitControls
          ref={controlsRef}
          makeDefault
          target={DEFAULT_CAM_TARGET}
          maxPolarAngle={Math.PI / 2 - 0.04} // Prevent camera going below floor
          minDistance={1.0}
          maxDistance={45}
          enableDamping
          dampingFactor={0.07}
          panSpeed={1.2}
          rotateSpeed={0.8}
          zoomSpeed={1.2}
          mouseButtons={{
            LEFT: THREE.MOUSE.ROTATE, // Left click: 3D CAD Orbit & Game Look
            MIDDLE: THREE.MOUSE.DOLLY, // Wheel / Middle click: Smooth Zoom
            RIGHT: THREE.MOUSE.PAN // Right click: CAD Pan across floor
          }}
        />

        {/* ── WASD & ARROW KEYS 3D GAME FLIGHT CONTROLLER ── */}
        <GameCameraController
          cameraMode={cameraMode}
          roverPosition={position}
          focusRoom={focusRoom}
          resetTrigger={resetTrigger}
          controlsRef={controlsRef}
          onKeysChange={onKeysChange}
        />

        {/* ── NATURAL CLINICAL DAYLIGHT LIGHTING ── */}
        <ambientLight intensity={isWireframe ? 0.6 : 1.2} />
        <directionalLight
          position={[12, 25, 12]}
          intensity={1.5}
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={45}
          shadow-camera-left={-18}
          shadow-camera-right={18}
          shadow-camera-top={18}
          shadow-camera-bottom={-18}
          shadow-bias={-0.0001}
        />
        <directionalLight position={[-10, 15, -8]} intensity={0.5} color="#f8fafc" />

        {/* ── RAYCASTING GROUND PLANE FOR LIVE CAD MOUSE CURSOR ── */}
        <mesh
          position={[0, 0.001, 0]}
          rotation={[-Math.PI / 2, 0, 0]}
          onPointerMove={handlePointerMove}
          visible={false}
        >
          <planeGeometry args={[40, 30]} />
        </mesh>

        {/* ── CAD BLUEPRINT GUIDES & METRIC AXES (Only in Wireframe mode) ── */}
        {isWireframe && <CadBlueprintGuides />}

        {/* ── 4 HOSPITAL BAYS & CORRIDOR INFRASTRUCTURE ── */}
        <CorridorLayout
          activeRoom={activeRoom}
          onSelectRoom={onSelectRoom}
          activeRoutePoints={activeRoutePoints}
          isWireframe={isWireframe}
        />

        {/* ── 3D MEDROVER ROBOT MODEL ── */}
        <RoverModel
          targetPosition={position}
          yaw={yaw}
          steeringAngle={steeringAngle}
          isLidOpen={isLidOpen}
          state={state}
          battery={battery}
          speed={speed}
          isWireframe={isWireframe}
        />

        {/* ── REALISTIC GROUND CONTACT SHADOW ── */}
        <ContactShadows
          position={[0, 0.005, 0]}
          opacity={isWireframe ? 0.1 : 0.3}
          scale={30}
          blur={1.6}
          far={5}
          color="#334155"
        />
      </Canvas>
    </div>
  )
}
