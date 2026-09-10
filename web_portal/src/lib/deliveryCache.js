/**
 * deliveryCache.js — Delivery Cache Memory System
 *
 * Keeps active delivery state, room assignments, and order histories
 * cached in browser memory (localStorage) so operations continue uninterrupted
 * if the network/internet drops mid-delivery.
 */

import { enqueue } from './syncQueue'

const STORAGE_KEYS = {
  ACTIVE_DELIVERY : 'medrover_active_delivery',
  ALL_ORDERS      : 'medrover_orders_cache',
  USER_ORDERS     : 'medrover_user_orders_',
  OFFLINE_COUNTER : 'medrover_offline_counter',
}

// ── Helpers ──────────────────────────────────────────────────────────────────

function safeGet(key, fallback = null) {
  try {
    const raw = localStorage.getItem(key)
    return raw ? JSON.parse(raw) : fallback
  } catch (e) {
    console.warn('[DeliveryCache] Read error for key:', key, e)
    return fallback
  }
}

function safeSet(key, value) {
  try {
    localStorage.setItem(key, JSON.stringify(value))
  } catch (e) {
    console.warn('[DeliveryCache] Write error for key:', key, e)
  }
}

function dispatchCacheEvent(eventName, detail = {}) {
  window.dispatchEvent(new CustomEvent(eventName, { detail }))
}

// ── Active Delivery Cache (In-flight Delivery Memory) ─────────────────────────

/**
 * Cache the currently active in-flight delivery.
 * Called when an order transitions to 'in_transit' or is created.
 */
export function cacheActiveDelivery(order) {
  if (!order) {
    clearActiveDelivery()
    return
  }
  const cached = {
    ...order,
    cachedAt: Date.now(),
    isCached: true,
  }
  safeSet(STORAGE_KEYS.ACTIVE_DELIVERY, cached)
  dispatchCacheEvent('medrover_active_delivery_changed', cached)
}

/**
 * Get the currently active delivery from cache memory.
 */
export function getActiveDelivery() {
  return safeGet(STORAGE_KEYS.ACTIVE_DELIVERY, null)
}

/**
 * Clear the active delivery when completed or cancelled.
 */
export function clearActiveDelivery() {
  localStorage.removeItem(STORAGE_KEYS.ACTIVE_DELIVERY)
  dispatchCacheEvent('medrover_active_delivery_changed', null)
}

// ── All Orders Cache (Admin / Global) ─────────────────────────────────────────

/**
 * Cache the list of orders in local memory.
 */
export function cacheOrders(orders) {
  if (!Array.isArray(orders)) return
  safeSet(STORAGE_KEYS.ALL_ORDERS, orders)
  dispatchCacheEvent('medrover_orders_cache_changed', orders)
}

/**
 * Retrieve cached orders from local memory.
 */
export function getCachedOrders() {
  return safeGet(STORAGE_KEYS.ALL_ORDERS, [])
}

/**
 * Update an order's status in the local cache memory immediately.
 * If this order is the active delivery, update active delivery cache as well.
 */
export function updateCachedOrderStatus(orderId, newStatus) {
  const orders = getCachedOrders()
  let updated = false
  const updatedOrders = orders.map(o => {
    if (String(o.id) === String(orderId)) {
      updated = true
      return { ...o, status: newStatus, updated_at: new Date().toISOString() }
    }
    return o
  })

  if (updated) {
    cacheOrders(updatedOrders)
  }

  // Also check active delivery cache
  const active = getActiveDelivery()
  if (active && String(active.id) === String(orderId)) {
    if (newStatus === 'delivered' || newStatus === 'cancelled') {
      clearActiveDelivery()
    } else {
      cacheActiveDelivery({ ...active, status: newStatus })
    }
  }

  return updatedOrders
}

/**
 * Remove an order from cached orders.
 */
export function removeCachedOrder(orderId) {
  const orders = getCachedOrders()
  const filtered = orders.filter(o => String(o.id) !== String(orderId))
  cacheOrders(filtered)

  const active = getActiveDelivery()
  if (active && String(active.id) === String(orderId)) {
    clearActiveDelivery()
  }
}

// ── User Orders Cache ─────────────────────────────────────────────────────────

export function cacheUserOrders(userId, orders) {
  if (!userId || !Array.isArray(orders)) return
  safeSet(STORAGE_KEYS.USER_ORDERS + userId, orders)
}

export function getCachedUserOrders(userId) {
  if (!userId) return []
  return safeGet(STORAGE_KEYS.USER_ORDERS + userId, [])
}

// ── Offline Order Creation (Saved Directly to Cache Memory) ──────────────────

/**
 * Generates an offline order when internet is lost and immediately caches it.
 * Enqueues CREATE_ORDER for Supabase background sync.
 */
export function saveOfflineOrder(orderPayload, itemsPayload = []) {
  // Generate local unique ID
  const count = (parseInt(localStorage.getItem(STORAGE_KEYS.OFFLINE_COUNTER) || '100', 10) + 1)
  localStorage.setItem(STORAGE_KEYS.OFFLINE_COUNTER, String(count))
  const localId = `off_${Date.now().toString().slice(-4)}_${count}`

  const now = new Date().toISOString()
  const offlineOrder = {
    id: localId,
    ...orderPayload,
    created_at: now,
    status: orderPayload.status || 'pending',
    is_offline: true,
  }

  // 1. Add to active orders cache
  const currentOrders = getCachedOrders()
  cacheOrders([offlineOrder, ...currentOrders])

  // 2. Add to user orders cache
  if (orderPayload.user_id) {
    const userOrders = getCachedUserOrders(orderPayload.user_id)
    cacheUserOrders(orderPayload.user_id, [offlineOrder, ...userOrders])
  }

  // 3. Set active delivery cache
  cacheActiveDelivery(offlineOrder)

  // 4. Queue for Supabase upload once network restores
  enqueue('CREATE_ORDER', {
    localId,
    orderPayload,
    itemsPayload,
  })

  return offlineOrder
}
