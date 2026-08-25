import { useState, useEffect, useRef } from 'react'
import { supabase } from '../lib/supabaseClient'
import { TOPOLOGY } from '../components/CorridorLayout'

export function useRoverRealtime() {
  const [roverState, setRoverState] = useState({
    position: [0, 0.2, 0],
    yaw: 0,
    state: 'idle', // idle, in_transit, delivered, returning_to_base, error
    activeRoom: null, // 'A', 'B', 'C'
    battery: 94,
    wifiRssi: -58,
    orderId: null,
    speed: 0,
    progress: 0
  })

  const [isRealtimeConnected, setIsRealtimeConnected] = useState(false)
  const [isSimulating, setIsSimulating] = useState(true)

  // Ref to store active path sequence for simulation
  const waypointsRef = useRef([])
  const currentWaypointIdxRef = useRef(0)
  const animTimerRef = useRef(null)

  // ── 1. SUPABASE REALTIME SUBSCRIPTION ──
  useEffect(() => {
    if (!supabase) return

    // Subscribe to changes in rover_status table
    const roverChannel = supabase
      .channel('rover_status_changes')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'rover_status' },
        (payload) => {
          console.log('[Supabase Realtime] rover_status event:', payload)
          setIsRealtimeConnected(true)
          const newStatus = payload.new
          if (newStatus) {
            setRoverState((prev) => ({
              ...prev,
              battery: newStatus.battery ?? prev.battery,
              wifiRssi: newStatus.wifi_rssi ?? prev.wifiRssi,
              state: newStatus.state ?? prev.state,
              activeRoom: newStatus.target_room ?? prev.activeRoom,
              orderId: newStatus.order_id ?? prev.orderId,
              yaw: newStatus.yaw ?? prev.yaw,
              position: [
                newStatus.pos_x ?? prev.position[0],
                0.2,
                newStatus.pos_z ?? prev.position[2]
              ]
            }))
          }
        }
      )
      .subscribe((status) => {
        if (status === 'SUBSCRIBED') {
          setIsRealtimeConnected(true)
        }
      })

    // Subscribe to new orders being placed
    const ordersChannel = supabase
      .channel('orders_changes')
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'orders' },
        (payload) => {
          console.log('[Supabase Realtime] New Order Created:', payload)
          const newOrder = payload.new
          if (newOrder && newOrder.room_code) {
            // Auto dispatch mission to room specified in new order
            dispatchMission(newOrder.room_code, newOrder.id)
          }
        }
      )
      .subscribe()

    return () => {
      supabase.removeChannel(roverChannel)
      supabase.removeChannel(ordersChannel)
    }
  }, [])

  // ── 2. PATH WAYPOINT GENERATOR ──
  const generatePathWaypoints = (roomCode) => {
    const points = []
    const jKey = roomCode === 'A' ? 'J1' : roomCode === 'B' ? 'J2' : 'J3'
    const roomKey = roomCode === 'A' ? 'ROOM_A' : roomCode === 'B' ? 'ROOM_B' : 'ROOM_C'

    const junction = TOPOLOGY[jKey]
    const room = TOPOLOGY[roomKey]

    // Step 1: Start at Base (0, 0)
    points.push({ x: TOPOLOGY.BASE.x, z: TOPOLOGY.BASE.z, phase: 'in_transit', yaw: 180 })

    // Step 2: Intermediate Junctions along Spine
    if (roomCode === 'B' || roomCode === 'C') {
      points.push({ x: TOPOLOGY.J1.x, z: TOPOLOGY.J1.z, phase: 'in_transit', yaw: 180 })
    }
    if (roomCode === 'C') {
      points.push({ x: TOPOLOGY.J2.x, z: TOPOLOGY.J2.z, phase: 'in_transit', yaw: 180 })
    }

    // Step 3: Target Junction
    points.push({ x: junction.x, z: junction.z, phase: 'in_transit', yaw: 180 })

    // Step 4: Turn Left into Spur to Room
    points.push({ x: room.x, z: room.z, phase: 'delivered', yaw: -90 })

    // Step 5: Return Back to Junction
    points.push({ x: junction.x, z: junction.z, phase: 'returning_to_base', yaw: 90 })

    // Step 6: Return Back along Spine to Base
    if (roomCode === 'C') {
      points.push({ x: TOPOLOGY.J2.x, z: TOPOLOGY.J2.z, phase: 'returning_to_base', yaw: 0 })
    }
    if (roomCode === 'B' || roomCode === 'C') {
      points.push({ x: TOPOLOGY.J1.x, z: TOPOLOGY.J1.z, phase: 'returning_to_base', yaw: 0 })
    }

    // Step 7: Return to Dock Base
    points.push({ x: TOPOLOGY.BASE.x, z: TOPOLOGY.BASE.z, phase: 'idle', yaw: 0 })

    return points
  }

  // ── 3. DISPATCH MISSION FUNCTION ──
  const dispatchMission = (roomCode, orderId = null) => {
    const waypoints = generatePathWaypoints(roomCode)
    waypointsRef.current = waypoints
    currentWaypointIdxRef.current = 0

    setRoverState((prev) => ({
      ...prev,
      activeRoom: roomCode,
      orderId: orderId || `ORD-${Math.floor(1000 + Math.random() * 9000)}`,
      state: 'in_transit',
      progress: 5
    }))

    if (animTimerRef.current) clearInterval(animTimerRef.current)

    // Waypoint Tick Interval
    animTimerRef.current = setInterval(() => {
      currentWaypointIdxRef.current += 1
      const idx = currentWaypointIdxRef.current
      const currentPoints = waypointsRef.current

      if (idx >= currentPoints.length) {
        clearInterval(animTimerRef.current)
        setRoverState((prev) => ({
          ...prev,
          position: [0, 0.2, 0],
          yaw: 0,
          state: 'idle',
          activeRoom: null,
          progress: 100
        }))
        return
      }

      const wp = currentPoints[idx]
      const totalSteps = currentPoints.length
      const progressPct = Math.round((idx / (totalSteps - 1)) * 100)

      setRoverState((prev) => ({
        ...prev,
        position: [wp.x, 0.2, wp.z],
        yaw: wp.yaw,
        state: wp.phase,
        progress: progressPct,
        battery: Math.max(15, prev.battery - 0.5)
      }))

      // Also optionally update Supabase rover_status table
      if (supabase && !isSimulating) {
        supabase
          .from('rover_status')
          .upsert({
            id: 1,
            pos_x: wp.x,
            pos_z: wp.z,
            yaw: wp.yaw,
            state: wp.phase,
            target_room: roomCode,
            updated_at: new Date().toISOString()
          })
          .catch((err) => console.warn('Supabase status sync fallback:', err))
      }
    }, 2200)
  }

  // Return to base
  const returnToBase = () => {
    if (animTimerRef.current) clearInterval(animTimerRef.current)
    setRoverState((prev) => ({
      ...prev,
      position: [0, 0.2, 0],
      yaw: 0,
      state: 'idle',
      activeRoom: null,
      progress: 0
    }))
  }

  // Emergency stop
  const emergencyStop = () => {
    if (animTimerRef.current) clearInterval(animTimerRef.current)
    setRoverState((prev) => ({
      ...prev,
      state: 'error'
    }))
  }

  return {
    roverState,
    dispatchMission,
    returnToBase,
    emergencyStop,
    isRealtimeConnected,
    isSimulating,
    setIsSimulating
  }
}
