/**
 * MedRover Service Worker
 *
 * Strategy:
 * - App Shell (HTML, CSS, JS, assets): Cache-first — loads instantly offline
 * - Supabase API calls: Network-first with fallback to cache
 * - Background Sync: Replays the localStorage syncQueue when online
 */

const CACHE_NAME    = 'medrover-v1'
const SUPABASE_HOST = 'supabase.co'

// Files to pre-cache on install (app shell)
const APP_SHELL = [
  '/',
  '/index.html',
]

// ── Install: pre-cache the app shell ─────────────────────────────────────────
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(APP_SHELL))
  )
  self.skipWaiting()
})

// ── Activate: clean up old caches ─────────────────────────────────────────────
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k)))
    )
  )
  self.clients.claim()
})

// ── Fetch: intercept network requests ────────────────────────────────────────
self.addEventListener('fetch', event => {
  const { request } = event
  const url = new URL(request.url)

  // 1. Supabase API → Network-first, fall back to cache
  if (url.hostname.includes(SUPABASE_HOST)) {
    event.respondWith(networkFirst(request))
    return
  }

  // 2. Navigation requests (HTML pages) → Network-first, fall back to /
  if (request.mode === 'navigate') {
    event.respondWith(
      fetch(request).catch(() =>
        caches.match('/index.html')
      )
    )
    return
  }

  // 3. Static assets (JS, CSS, images) → Cache-first
  event.respondWith(cacheFirst(request))
})

async function networkFirst(request) {
  try {
    const response = await fetch(request)
    // Cache successful GET responses
    if (request.method === 'GET' && response.ok) {
      const cache = await caches.open(CACHE_NAME)
      cache.put(request, response.clone())
    }
    return response
  } catch {
    return caches.match(request) || new Response(
      JSON.stringify({ error: 'offline' }),
      { status: 503, headers: { 'Content-Type': 'application/json' } }
    )
  }
}

async function cacheFirst(request) {
  const cached = await caches.match(request)
  if (cached) return cached
  try {
    const response = await fetch(request)
    if (response.ok) {
      const cache = await caches.open(CACHE_NAME)
      cache.put(request, response.clone())
    }
    return response
  } catch {
    return new Response('Offline', { status: 503 })
  }
}

// ── Background Sync ───────────────────────────────────────────────────────────
self.addEventListener('sync', event => {
  if (event.tag === 'medrover-sync') {
    // Notify all open clients to process their sync queue
    event.waitUntil(
      self.clients.matchAll().then(clients => {
        clients.forEach(client =>
          client.postMessage({ type: 'TRIGGER_SYNC' })
        )
      })
    )
  }
})

// ── Push notifications (future use) ──────────────────────────────────────────
self.addEventListener('push', event => {
  if (!event.data) return
  const data = event.data.json()
  event.waitUntil(
    self.registration.showNotification(data.title || 'MedRover', {
      body:  data.body  || 'Delivery update',
      icon:  '/assets/logo/white.png',
      badge: '/assets/logo/white.png',
    })
  )
})
