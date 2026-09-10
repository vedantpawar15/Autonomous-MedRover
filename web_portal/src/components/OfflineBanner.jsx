import { useState, useEffect, useCallback } from 'react'
import { useOnlineStatus } from '../hooks/useOnlineStatus'
import { getPendingCount, processQueue } from '../lib/syncQueue'
import { supabase } from '../lib/supabaseClient'

/**
 * OfflineBanner
 *
 * Shows a persistent banner at the top of the screen when offline,
 * displaying how many operations are pending sync.
 * When the connection is restored it auto-syncs and shows a success flash.
 */
export default function OfflineBanner() {
  const { isOnline }          = useOnlineStatus()
  const [pendingCount, setPendingCount] = useState(getPendingCount)
  const [syncState, setSyncState]       = useState('idle') // 'idle' | 'syncing' | 'done' | 'error'
  const [visible, setVisible]           = useState(false)

  // Refresh pending count periodically (in case AdminPage adds to queue)
  useEffect(() => {
    const id = setInterval(() => setPendingCount(getPendingCount()), 1000)
    return () => clearInterval(id)
  }, [])

  // Show banner when offline OR when there's a pending count
  useEffect(() => {
    if (!isOnline || pendingCount > 0) setVisible(true)
    else if (syncState === 'done') {
      // Keep the "synced" message visible for 3 s then hide
      const t = setTimeout(() => setVisible(false), 3000)
      return () => clearTimeout(t)
    } else {
      setVisible(false)
    }
  }, [isOnline, pendingCount, syncState])

  // Auto-sync when coming back online
  const runSync = useCallback(async () => {
    if (!supabase || pendingCount === 0) return
    setSyncState('syncing')
    try {
      const { synced, failed } = await processQueue(supabase)
      setPendingCount(getPendingCount())
      setSyncState(failed === 0 ? 'done' : 'error')
    } catch {
      setSyncState('error')
    }
  }, [pendingCount])

  useEffect(() => {
    if (isOnline && pendingCount > 0) runSync()
    if (isOnline && pendingCount === 0) setSyncState('idle')
  }, [isOnline]) // eslint-disable-line react-hooks/exhaustive-deps

  if (!visible) return null

  // ── Styles per state ──────────────────────────────────────────────────────
  let bgColor, icon, message

  if (!isOnline) {
    bgColor = '#1a2e38'
    icon    = 'bi-wifi-off'
    message = pendingCount > 0
      ? `No internet — ${pendingCount} delivery update${pendingCount > 1 ? 's' : ''} queued for sync`
      : 'No internet connection — delivery data may be outdated'

  } else if (syncState === 'syncing') {
    bgColor = '#1E7F78'
    icon    = 'bi-arrow-repeat'
    message = `Syncing ${pendingCount} pending update${pendingCount > 1 ? 's' : ''}…`

  } else if (syncState === 'done') {
    bgColor = '#166534'
    icon    = 'bi-check-circle-fill'
    message = 'Back online — all delivery updates synced successfully ✅'

  } else if (syncState === 'error') {
    bgColor = '#7f1d1d'
    icon    = 'bi-exclamation-triangle-fill'
    message = `Back online — ${getPendingCount()} update${getPendingCount() > 1 ? 's' : ''} failed to sync. Will retry.`

  } else {
    // Online, pending ops remain (shouldn't normally happen)
    bgColor = '#854d0e'
    icon    = 'bi-cloud-upload'
    message = `${pendingCount} update${pendingCount > 1 ? 's' : ''} pending sync`
  }

  return (
    <div className="offline-banner" style={{ background: bgColor }}>
      <i className={`bi ${icon}${syncState === 'syncing' ? ' offline-spin' : ''}`} />
      <span>{message}</span>
      {pendingCount > 0 && isOnline && syncState !== 'syncing' && (
        <button className="offline-retry-btn" onClick={runSync}>
          <i className="bi bi-arrow-clockwise" /> Retry now
        </button>
      )}
    </div>
  )
}
