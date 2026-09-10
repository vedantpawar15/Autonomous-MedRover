import { useState, useEffect } from 'react'

/**
 * useOnlineStatus
 * Returns { isOnline } — true when the browser has network connectivity.
 * Subscribes to the native online/offline events so React re-renders
 * the moment the connection changes.
 */
export function useOnlineStatus() {
  const [isOnline, setIsOnline] = useState(() => navigator.onLine)

  useEffect(() => {
    const handleOnline  = () => setIsOnline(true)
    const handleOffline = () => setIsOnline(false)

    window.addEventListener('online',  handleOnline)
    window.addEventListener('offline', handleOffline)

    return () => {
      window.removeEventListener('online',  handleOnline)
      window.removeEventListener('offline', handleOffline)
    }
  }, [])

  return { isOnline }
}
