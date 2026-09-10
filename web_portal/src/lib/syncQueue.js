/**
 * syncQueue.js — Offline Sync Queue Engine
 *
 * Stores failed Supabase write operations in localStorage so they can be
 * replayed automatically when the internet connection is restored.
 *
 * Queue entry shape:
 * {
 *   id:        string   (random key)
 *   type:      string   (e.g. 'UPDATE_ORDER_STATUS')
 *   payload:   object   (operation-specific data)
 *   createdAt: number   (Date.now())
 *   retries:   number   (replay attempt count)
 * }
 */

const QUEUE_KEY = 'medrover_sync_queue'

// ── Helpers ──────────────────────────────────────────────────────────────────

function randomId() {
  return Math.random().toString(36).slice(2) + Date.now().toString(36)
}

/** Read the full queue from localStorage. */
export function getQueue() {
  try {
    return JSON.parse(localStorage.getItem(QUEUE_KEY) || '[]')
  } catch {
    return []
  }
}

/** Persist the full queue to localStorage. */
function saveQueue(queue) {
  localStorage.setItem(QUEUE_KEY, JSON.stringify(queue))
}

/** Count of pending operations. */
export function getPendingCount() {
  return getQueue().length
}

// ── Enqueue ───────────────────────────────────────────────────────────────────

/**
 * Add a failed operation to the sync queue.
 * @param {string} type    Operation type identifier
 * @param {object} payload Data needed to replay the operation
 * @returns {string}       The generated entry ID
 */
export function enqueue(type, payload) {
  const queue = getQueue()
  const entry = { id: randomId(), type, payload, createdAt: Date.now(), retries: 0 }
  queue.push(entry)
  saveQueue(queue)

  // Notify the service worker (if registered) to schedule a background sync
  if ('serviceWorker' in navigator && 'SyncManager' in window) {
    navigator.serviceWorker.ready
      .then(reg => reg.sync.register('medrover-sync'))
      .catch(() => {}) // silently ignore if SW not active yet
  }

  return entry.id
}

/** Remove a single entry from the queue by id. */
export function dequeue(id) {
  const queue = getQueue().filter(e => e.id !== id)
  saveQueue(queue)
}

/** Clear all queued operations. */
export function clearQueue() {
  localStorage.removeItem(QUEUE_KEY)
}

// ── Replay ────────────────────────────────────────────────────────────────────

/**
 * Replay all queued operations against Supabase.
 * Removes entries that succeed. Increments retries for failures.
 *
 * @param {import('@supabase/supabase-js').SupabaseClient} supabase
 * @returns {Promise<{ synced: number, failed: number }>}
 */
export async function processQueue(supabase) {
  const queue = getQueue()
  if (queue.length === 0) return { synced: 0, failed: 0 }

  let synced = 0
  let failed = 0
  const remaining = []

  for (const entry of queue) {
    let success = false

    try {
      if (entry.type === 'UPDATE_ORDER_STATUS') {
        const { orderId, status } = entry.payload
        const { error } = await supabase
          .from('orders')
          .update({ status })
          .eq('id', orderId)
        success = !error

      } else if (entry.type === 'CREATE_ORDER') {
        const { orderPayload, itemsPayload } = entry.payload
        const { data: createdOrder, error: oErr } = await supabase
          .from('orders')
          .insert(orderPayload)
          .select('*')
          .single()

        if (!oErr && createdOrder) {
          if (itemsPayload && itemsPayload.length > 0) {
            const mappedItems = itemsPayload.map(item => ({
              ...item,
              order_id: createdOrder.id
            }))
            await supabase.from('order_items').insert(mappedItems)
          }
          success = true
        } else {
          success = false
        }

      } else if (entry.type === 'DELETE_ORDER') {
        const { orderId } = entry.payload
        await supabase.from('order_items').delete().eq('order_id', orderId)
        const { error } = await supabase.from('orders').delete().eq('id', orderId)
        success = !error

      } else if (entry.type === 'UPDATE_MEDICINE') {
        const { id, payload: medPayload } = entry.payload
        const { error } = await supabase.from('medicines').update(medPayload).eq('id', id)
        success = !error

      } else if (entry.type === 'INSERT_MEDICINE') {
        const { error } = await supabase.from('medicines').insert(entry.payload)
        success = !error

      } else if (entry.type === 'DELETE_MEDICINE') {
        const { error } = await supabase.from('medicines').delete().eq('id', entry.payload.id)
        success = !error
      }

    } catch {
      success = false
    }

    if (success) {
      synced++
    } else {
      failed++
      remaining.push({ ...entry, retries: entry.retries + 1 })
    }
  }

  saveQueue(remaining)
  return { synced, failed }
}
