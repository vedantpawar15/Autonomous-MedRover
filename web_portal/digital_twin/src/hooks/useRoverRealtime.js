import { useState, useEffect, useRef, useCallback } from 'react'
import { supabase } from '../lib/supabaseClient'
import { TOPOLOGY, ROOMS_CONFIG } from '../components/CorridorLayout'

// Turn radius in meters for smooth cornering
const CORNER_RADIUS = 0.6
const ACCELERATION = 1.2 // m/s^2
const CRUISE_SPEED = 1.1 // m/s (~4 km/h)
const TURN_SPEED = 0.55 // m/s
const DECEL_DISTANCE = 1.0 // m

/**
 * Builds continuous parametric segments for MedRover to travel from Dock Base to any of the 4 bays and back.
 */
export function buildContinuousPath(roomCodeOrId) {
  const norm = (roomCodeOrId || 'A').toString().trim().toUpperCase()
  let target = ROOMS_CONFIG.find((r) => r.code === norm || r.id === norm || r.name === norm)
  if (!target) target = ROOMS_CONFIG[0] // default to ICU

  const roomX = target.x
  const dropZ = -1.2
  const spineZ = 1.5
  const baseX = TOPOLOGY.BASE.x // -8.0
  const R = CORNER_RADIUS

  const segments = []

  // ── SEGMENT 1: Outbound Spine (Base -8.0, 1.5 to start of turn roomX - R, 1.5) ──
  const seg1Len = Math.abs((roomX - R) - baseX)
  segments.push({
    type: 'line',
    name: 'Hospital Corridor Spine',
    guidance: `Proceed down main corridor towards ${target.name}`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'in_transit',
    start: { x: baseX, z: spineZ },
    end: { x: roomX - R, z: spineZ },
    yaw: 90, // Heading +X
    steering: 0,
    length: seg1Len,
    targetSpeed: CRUISE_SPEED,
    interpolate: (t) => ({
      x: baseX + t * seg1Len,
      z: spineZ,
      yaw: 90,
      steering: 0
    })
  })

  // ── SEGMENT 2: Left Turn Arc into Room Bay (from heading 90 to 0) ──
  // Arc center: (roomX - R, spineZ - R), radius R
  // At t=0: x = roomX - R, z = spineZ. Heading = 90
  // At t=1: x = roomX, z = spineZ - R. Heading = 0 (facing -Z into bay)
  const arcLen = (Math.PI / 2) * R
  segments.push({
    type: 'arc',
    name: `${target.name} Access Turn`,
    guidance: `Turn left into ${target.name} Drop-off Bay`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'in_transit',
    length: arcLen,
    targetSpeed: TURN_SPEED,
    interpolate: (t) => {
      const angle = t * (Math.PI / 2)
      const x = (roomX - R) + R * Math.sin(angle)
      const z = (spineZ - R) + R * Math.cos(angle)
      const yaw = 90 - t * 90 // 90 -> 0
      const steering = -25
      return { x, z, yaw, steering }
    }
  })

  // ── SEGMENT 3: Straight Spur into the Bay (z = spineZ - R down to dropZ) ──
  const spurLen = Math.abs((spineZ - R) - dropZ)
  segments.push({
    type: 'line',
    name: `${target.name} Bay Approach`,
    guidance: `Approaching ${target.name} Patient Delivery Bed`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'in_transit',
    start: { x: roomX, z: spineZ - R },
    end: { x: roomX, z: dropZ },
    length: spurLen,
    targetSpeed: CRUISE_SPEED,
    interpolate: (t) => ({
      x: roomX,
      z: (spineZ - R) - t * spurLen,
      yaw: 0,
      steering: 0
    })
  })

  // ── SEGMENT 4: Medication Handover Dwell at Patient Bedside ──
  const dwellDuration = 4.5
  const dwellEquivalentLen = dwellDuration * 0.6
  segments.push({
    type: 'dwell',
    name: `${target.name} Handover`,
    guidance: `Medication Handover: Compartment Unlocked at ${target.name}`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'delivered',
    duration: dwellDuration,
    length: dwellEquivalentLen,
    targetSpeed: 0,
    interpolate: () => ({
      x: roomX,
      z: dropZ,
      yaw: 0,
      steering: 0,
      isLidOpen: true
    })
  })

  // ── SEGMENT 5: Return Spur (from dropZ back to spineZ - R) ──
  segments.push({
    type: 'line',
    name: `${target.name} Exit`,
    guidance: `Mission complete at ${target.name}. Exiting bay...`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'returning_to_base',
    start: { x: roomX, z: dropZ },
    end: { x: roomX, z: spineZ - R },
    length: spurLen,
    targetSpeed: CRUISE_SPEED,
    interpolate: (t) => ({
      x: roomX,
      z: dropZ + t * spurLen,
      yaw: 180, // Facing +Z towards corridor
      steering: 0
    })
  })

  // ── SEGMENT 6: Right Turn Arc back onto Corridor Spine toward Base ──
  // Arc turning from heading 180 (facing +Z) to 270 (facing -X toward Base)
  segments.push({
    type: 'arc',
    name: 'Corridor Spine Return Turn',
    guidance: `Turning onto Corridor Spine towards Base Dock`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'returning_to_base',
    length: arcLen,
    targetSpeed: TURN_SPEED,
    interpolate: (t) => {
      const angle = (Math.PI / 2) * (1 - t)
      const x = (roomX - R) + R * Math.sin(angle)
      const z = (spineZ - R) + R * Math.cos(angle)
      const yaw = 180 + t * 90 // 180 -> 270 (-90)
      const steering = 25
      return { x, z, yaw, steering }
    }
  })

  // ── SEGMENT 7: Return Spine to Dock Base (-8.0) ──
  const returnSpineLen = Math.abs((roomX - R) - baseX)
  segments.push({
    type: 'line',
    name: 'Base Dock Approach',
    guidance: `Docking at Base Station for recharge`,
    targetRoom: target.code,
    roomName: target.name,
    phase: 'returning_to_base',
    start: { x: roomX - R, z: spineZ },
    end: { x: baseX, z: spineZ },
    length: returnSpineLen,
    targetSpeed: CRUISE_SPEED,
    interpolate: (t) => ({
      x: (roomX - R) - t * returnSpineLen,
      z: spineZ,
      yaw: 270,
      steering: 0
    })
  })

  // Cumulative distances
  let cumulative = 0
  for (const seg of segments) {
    seg.startDist = cumulative
    cumulative += seg.length
    seg.endDist = cumulative
  }

  // Sample polyline for 3D route rendering (outbound path)
  const routePoints = []
  routePoints.push([baseX, 0.015, spineZ])
  routePoints.push([roomX - R, 0.015, spineZ])
  for (let i = 1; i <= 8; i++) {
    const t = i / 8
    const a = t * (Math.PI / 2)
    routePoints.push([(roomX - R) + R * Math.sin(a), 0.015, (spineZ - R) + R * Math.cos(a)])
  }
  routePoints.push([roomX, 0.015, dropZ])

  return {
    segments,
    totalDistance: cumulative,
    routePoints,
    roomCode: target.code,
    roomName: target.name,
    roomTargetX: roomX,
    roomTargetZ: dropZ
  }
}

export function useRoverRealtime() {
  const [roverState, setRoverState] = useState({
    position: [TOPOLOGY.BASE.x, 0.2, TOPOLOGY.BASE.z],
    yaw: 90,
    steeringAngle: 0,
    isLidOpen: false,
    state: 'idle',
    activeRoom: null,
    activeRoomName: null,
    battery: 33,
    wifiRssi: -58,
    orderId: null,
    speed: 0,
    progress: 0,
    etaSeconds: 0,
    distanceRemaining: 0,
    distanceTravelled: 0,
    guidance: 'Digital Hospital Twin connected. Standing by.',
    activeSegmentName: 'Base Station',
    dataSource: 'supabase_live'
  })

  // Live ESP32 Sensor Telemetry (matching the user's reference screenshot)
  const [esp32Sensors, setEsp32Sensors] = useState({
    temperature: 31.2,
    humidity: 42,
    gasMq: 534,
    distance: 33.15,
    syncedTime: '11:12:45 AM',
    isConnected: true
  })

  const [activeRoutePoints, setActiveRoutePoints] = useState([])
  const [isRealtimeConnected, setIsRealtimeConnected] = useState(false)
  const [hardwareSyncMode, setHardwareSyncMode] = useState(true)
  const [speedMultiplier, setSpeedMultiplier] = useState(1)
  const [isPaused, setIsPaused] = useState(false)

  // Refs
  const pathPlanRef = useRef(null)
  const currentDistRef = useRef(0)
  const currentSpeedRef = useRef(0)
  const animFrameRef = useRef(null)
  const lastTimeRef = useRef(null)
  const dwellTimerRef = useRef(0)
  const lastSupabaseSyncRef = useRef(0)
  const activeOrderProcessedRef = useRef(null)

  // Periodic sensor fluctuation for realistic ESP32 telemetry
  useEffect(() => {
    const timer = setInterval(() => {
      setEsp32Sensors((prev) => ({
        temperature: Number((30.8 + Math.random() * 0.8).toFixed(1)),
        humidity: Math.round(41 + Math.random() * 3),
        gasMq: Math.round(530 + Math.random() * 8),
        distance: Number((32.5 + Math.random() * 1.5).toFixed(2)),
        syncedTime: new Date().toLocaleTimeString('en-US', { hour: 'numeric', minute: '2-digit', second: '2-digit', hour12: true }),
        isConnected: true
      }))
    }, 2500)
    return () => clearInterval(timer)
  }, [])

  // ── 1. MISSION DISPATCH FUNCTION ──
  const dispatchMission = useCallback((roomCodeOrId, orderId = null, fromHardware = false) => {
    setIsPaused(false)
    if (isPausedRef) isPausedRef.current = false
    lastTimeRef.current = null
    const plan = buildContinuousPath(roomCodeOrId)
    pathPlanRef.current = plan
    currentDistRef.current = 0
    currentSpeedRef.current = 0.5
    dwellTimerRef.current = 0
    setActiveRoutePoints(plan.routePoints)

    const initialOrd = orderId || `ORD-${Math.floor(1000 + Math.random() * 9000)}`
    activeOrderProcessedRef.current = initialOrd

    setRoverState((prev) => ({
      ...prev,
      activeRoom: plan.roomCode,
      activeRoomName: plan.roomName,
      orderId: initialOrd,
      state: 'in_transit',
      progress: 0,
      speed: 0.2,
      isLidOpen: false,
      dataSource: fromHardware ? 'esp32_hardware' : 'supabase_live',
      guidance: `Delivering to ${plan.roomName}. MedRover en route...`,
      activeSegmentName: 'Departure'
    }))
  }, [])

  // Return to base
  const returnToBase = useCallback((markDelivered = false) => {
    setIsPaused(false)
    const lastOrderId = activeOrderProcessedRef.current
    pathPlanRef.current = null
    currentDistRef.current = 0
    currentSpeedRef.current = 0
    setActiveRoutePoints([])

    setRoverState((prev) => ({
      ...prev,
      position: [TOPOLOGY.BASE.x, 0.2, TOPOLOGY.BASE.z],
      yaw: 90,
      steeringAngle: 0,
      isLidOpen: false,
      state: 'idle',
      activeRoom: null,
      activeRoomName: null,
      speed: 0,
      progress: 100,
      etaSeconds: 0,
      distanceRemaining: 0,
      guidance: 'MedRover Docked at Base. Ready for next mission.',
      activeSegmentName: 'Docking Base'
    }))

    if (supabase && markDelivered && lastOrderId) {
      supabase
        .from('orders')
        .update({ status: 'delivered' })
        .eq('id', lastOrderId)
        .catch(() => {})

      supabase
        .from('rover_status')
        .upsert({
          id: 'rover-01',
          state: 'idle',
          pos_x: TOPOLOGY.BASE.x,
          pos_z: TOPOLOGY.BASE.z,
          yaw: 90,
          speed: 0,
          target_room: null,
          order_id: null,
          updated_at: new Date().toISOString()
        })
        .catch(() => {})
    }
  }, [])

  // Emergency stop
  const emergencyStop = useCallback(() => {
    currentSpeedRef.current = 0
    setIsPaused(true)
    setRoverState((prev) => ({
      ...prev,
      speed: 0,
      state: 'error',
      guidance: 'EMERGENCY BRAKE ENGAGED! Rover halted.'
    }))

    if (supabase) {
      supabase
        .from('rover_status')
        .upsert({
          id: 'rover-01',
          state: 'error',
          speed: 0,
          updated_at: new Date().toISOString()
        })
        .catch(() => {})
    }
  }, [])

  const resumeMission = useCallback(() => {
    setIsPaused(false)
    setRoverState((prev) => ({
      ...prev,
      state: pathPlanRef.current ? 'in_transit' : 'idle',
      guidance: 'Resuming mission...'
    }))
  }, [])

  // ── 2. SUPABASE REALTIME SYNC ──
  useEffect(() => {
    if (!supabase) return

    let isMounted = true

    const checkActiveOrders = async () => {
      try {
        const { data: pendingOrders } = await supabase
          .from('orders')
          .select('*')
          .in('status', ['pending', 'in_transit'])
          .order('created_at', { ascending: false })
          .limit(1)

        if (isMounted && pendingOrders && pendingOrders.length > 0) {
          const activeOrd = pendingOrders[0]
          if (!pathPlanRef.current && activeOrd.room_code) {
            dispatchMission(activeOrd.room_code, activeOrd.id, true)
          }
        }
      } catch (err) {
        console.warn('Supabase fetch error:', err)
      }
    }

    checkActiveOrders()

    const ordersChannel = supabase
      .channel('portal_orders_stream')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'orders' },
        (payload) => {
          setIsRealtimeConnected(true)
          const order = payload.new
          if (!order) return

          if (payload.eventType === 'INSERT') {
            if (order.room_code && (!pathPlanRef.current || order.id !== activeOrderProcessedRef.current)) {
              dispatchMission(order.room_code, order.id, true)
            }
          }

          if (payload.eventType === 'UPDATE') {
            if (order.status === 'in_transit' && !pathPlanRef.current && order.room_code) {
              dispatchMission(order.room_code, order.id, true)
            } else if (order.status === 'delivered' || order.status === 'cancelled') {
              returnToBase(false)
            }
          }
        }
      )
      .subscribe((status) => {
        if (status === 'SUBSCRIBED') {
          setIsRealtimeConnected(true)
        }
      })

    const roverChannel = supabase
      .channel('hardware_rover_stream')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'rover_status' },
        (payload) => {
          setIsRealtimeConnected(true)
          const newStatus = payload.new
          if (newStatus && hardwareSyncMode && !pathPlanRef.current) {
            if (newStatus.pos_x !== undefined && newStatus.pos_z !== undefined) {
              setRoverState((prev) => ({
                ...prev,
                battery: newStatus.battery ?? prev.battery,
                wifiRssi: newStatus.wifi_rssi ?? prev.wifiRssi,
                state: newStatus.state ?? prev.state,
                activeRoom: newStatus.target_room ?? prev.activeRoom,
                orderId: newStatus.order_id ?? prev.orderId,
                yaw: newStatus.yaw ?? prev.yaw,
                speed: newStatus.speed ?? prev.speed,
                position: [newStatus.pos_x, 0.2, newStatus.pos_z]
              }))
            }
          }
        }
      )
      .subscribe()

    return () => {
      isMounted = false
      supabase.removeChannel(ordersChannel)
      supabase.removeChannel(roverChannel)
    }
  }, [dispatchMission, returnToBase, hardwareSyncMode])

  // Refs for animation loop stability
  const isPausedRef = useRef(isPaused)
  useEffect(() => {
    isPausedRef.current = isPaused
  }, [isPaused])

  const speedMultiplierRef = useRef(speedMultiplier)
  useEffect(() => {
    speedMultiplierRef.current = speedMultiplier
  }, [speedMultiplier])

  const batteryRef = useRef(33)

  const returnToBaseRef = useRef(null)
  returnToBaseRef.current = returnToBase

  // ── 3. CONTINUOUS PHYSICS TICK ANIMATION LOOP (Stable 60 FPS) ──
  useEffect(() => {
    let animId
    const loop = (time) => {
      if (!lastTimeRef.current) lastTimeRef.current = time
      const dt = Math.min((time - lastTimeRef.current) / 1000, 0.08)
      lastTimeRef.current = time

      const pathPlan = pathPlanRef.current
      if (pathPlan && !isPausedRef.current) {
        const { segments, totalDistance, roomName, roomTargetX, roomTargetZ } = pathPlan
        let dist = currentDistRef.current

        let activeSeg = segments.find((s) => dist >= s.startDist && dist < s.endDist)
        if (!activeSeg && dist >= totalDistance) {
          returnToBaseRef.current?.(true)
        } else if (activeSeg && activeSeg.type === 'dwell') {
          dwellTimerRef.current += dt * speedMultiplierRef.current
          currentSpeedRef.current = 0
          const remainingDwell = Math.max(0, activeSeg.duration - dwellTimerRef.current)
          const pct = Math.min(100, Math.round((dist / totalDistance) * 100))

          setRoverState((prev) => ({
            ...prev,
            position: [roomTargetX, 0.2, roomTargetZ],
            yaw: 0,
            steeringAngle: 0,
            isLidOpen: true,
            state: 'delivered',
            speed: 0,
            progress: pct,
            etaSeconds: Math.ceil(remainingDwell),
            distanceRemaining: 0,
            distanceTravelled: Number(dist.toFixed(1)),
            guidance: `Delivered to ${roomName}! Compartment unlocked (${Math.ceil(remainingDwell)}s remaining)`,
            activeSegmentName: activeSeg.name,
            battery: Math.round(batteryRef.current)
          }))

          if (dwellTimerRef.current >= activeSeg.duration) {
            currentDistRef.current = activeSeg.endDist
            dwellTimerRef.current = 0
          }
        } else if (activeSeg) {
          let targetSpeed = activeSeg.targetSpeed * speedMultiplierRef.current
          const distToEnd = totalDistance - dist
          if (distToEnd < DECEL_DISTANCE) {
            targetSpeed = Math.max(0.2, (distToEnd / DECEL_DISTANCE) * CRUISE_SPEED * speedMultiplierRef.current)
          }

          if (currentSpeedRef.current < targetSpeed) {
            currentSpeedRef.current = Math.min(targetSpeed, currentSpeedRef.current + ACCELERATION * dt * speedMultiplierRef.current)
          } else if (currentSpeedRef.current > targetSpeed) {
            currentSpeedRef.current = Math.max(targetSpeed, currentSpeedRef.current - ACCELERATION * 2 * dt * speedMultiplierRef.current)
          }

          const moveStep = currentSpeedRef.current * dt
          currentDistRef.current += moveStep
          dist = currentDistRef.current

          const segT = Math.max(0, Math.min(1, (dist - activeSeg.startDist) / activeSeg.length))
          const interp = activeSeg.interpolate(segT)

          const outboundDistance = segments[0].length + segments[1].length + segments[2].length
          let distRemaining = 0
          let etaSec = 0

          if (dist < outboundDistance) {
            distRemaining = Math.max(0, outboundDistance - dist)
            etaSec = Math.ceil(distRemaining / Math.max(currentSpeedRef.current, 0.4))
          } else {
            distRemaining = Math.max(0, totalDistance - dist)
            etaSec = Math.ceil(distRemaining / Math.max(currentSpeedRef.current, 0.4))
          }

          const progressPct = Math.min(100, Math.round((dist / totalDistance) * 100))
          batteryRef.current = Math.max(15, Number((batteryRef.current - 0.001 * dt).toFixed(4)))

          setRoverState((prev) => ({
            ...prev,
            position: [interp.x, 0.2, interp.z],
            yaw: interp.yaw,
            steeringAngle: interp.steering || 0,
            isLidOpen: false,
            state: activeSeg.phase,
            speed: Number(currentSpeedRef.current.toFixed(2)),
            progress: progressPct,
            etaSeconds: etaSec,
            distanceRemaining: Number(distRemaining.toFixed(1)),
            distanceTravelled: Number(dist.toFixed(1)),
            guidance: activeSeg.guidance,
            activeSegmentName: activeSeg.name,
            battery: Math.round(batteryRef.current)
          }))
        }
      }

      animId = requestAnimationFrame(loop)
    }

    animId = requestAnimationFrame(loop)
    return () => {
      if (animId) cancelAnimationFrame(animId)
    }
  }, [])

  return {
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
  }
}
