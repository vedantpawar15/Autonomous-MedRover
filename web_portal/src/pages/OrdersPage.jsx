import { useEffect, useState } from 'react'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'
import { cartTotalQty, readCartLines } from '../lib/cartStorage'
import { useAuth } from '../contexts/AuthContext'
import { cacheUserOrders, getCachedUserOrders } from '../lib/deliveryCache'
import { enqueue } from '../lib/syncQueue'

function OrdersPage() {
  const { user } = useAuth()
  const [orders, setOrders] = useState(() => getCachedUserOrders(user?.id))
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [cartCount, setCartCount] = useState(0)
  const [deletingId, setDeletingId] = useState(null)

  useEffect(() => {
    // 1. Instantly show cached orders from memory
    const cached = getCachedUserOrders(user?.id)
    if (cached && cached.length > 0) {
      setOrders(cached)
    }

    if (!supabase || !user?.id || !navigator.onLine) {
      return
    }

    const fetchOrders = async () => {
      setLoading(true)
      setError('')
      try {
        const { data, error: err } = await supabase
          .from('orders')
          .select('*')
          .eq('user_id', user?.id)
          .order('created_at', { ascending: false })
          .limit(50)

        if (err) {
          if (!cached || cached.length === 0) {
            setError('Could not load orders from server.')
          }
        } else {
          setOrders(data || [])
          cacheUserOrders(user?.id, data || [])
        }
      } catch (e) {
        if (!cached || cached.length === 0) {
          setError('Network disconnected. Displaying cached orders.')
        }
      } finally {
        setLoading(false)
      }
    }

    fetchOrders()
  }, [user?.id])

  const handleDeleteOrder = async (order) => {
    if (!supabase) {
      alert('Supabase is not configured. Please check your .env.')
      return
    }

    const ok = window.confirm(
      `Delete Order #${order.id}? This will also delete it from Supabase.`
    )
    if (!ok) return

    setDeletingId(order.id)
    setError('')

    // 1. Optimistic UI and Cache memory update
    const prev = orders
    const remaining = orders.filter((o) => o.id !== order.id)
    setOrders(remaining)
    cacheUserOrders(user?.id, remaining)

    // 2. If offline, enqueue and return cleanly
    if (!supabase || !navigator.onLine) {
      enqueue('DELETE_ORDER', { orderId: order.id })
      setDeletingId(null)
      return
    }

    try {
      const { error: itemsErr } = await supabase
        .from('order_items')
        .delete()
        .eq('order_id', order.id)

      if (itemsErr) throw itemsErr

      const { error: orderErr } = await supabase
        .from('orders')
        .delete()
        .eq('id', order.id)

      if (orderErr) throw orderErr
    } catch (e) {
      enqueue('DELETE_ORDER', { orderId: order.id })
    } finally {
      setDeletingId(null)
    }
  }

  useEffect(() => {
    const refresh = () => setCartCount(cartTotalQty(readCartLines()))
    refresh()
    window.addEventListener('medrover_cart_changed', refresh)
    return () => window.removeEventListener('medrover_cart_changed', refresh)
  }, [])

  return (
    <>
        <Navbar variant="plain" cartCount={cartCount} />

      <section className="select-room-section">
        <div className="container">
          <h4 className="mb-3">Recent Orders</h4>

          {loading && <p>Loading orders...</p>}
          {error && <p className="text-danger small">{error}</p>}

          {!loading && !error && orders.length === 0 && (
            <p className="small text-muted">No orders have been placed yet.</p>
          )}

          {!loading && !error && orders.length > 0 && (
            <div className="sidebar-card order-summary-card">
              {orders.map((order) => (
                <div className="order-summary-item d-flex justify-content-between align-items-start" key={order.id}>
                  <div>
                    <div className="fw-semibold">
                      Order #{order.id} &middot; Room {order.room_code}
                    </div>
                    <div className="small text-muted">
                      {order.room_label || 'No label'} &middot;{' '}
                      {new Date(order.created_at).toLocaleString()}
                    </div>
                  </div>
                  <div className="d-flex align-items-center gap-2">
                    <span className="badge bg-success-subtle text-success">
                      {order.status}
                    </span>
                    <button
                      type="button"
                      className="btn btn-sm btn-outline-danger"
                      onClick={() => handleDeleteOrder(order)}
                      disabled={deletingId === order.id}
                      title="Delete order"
                    >
                      {deletingId === order.id ? 'Deleting…' : 'Delete'}
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </section>

      <Footer variant="simple" />
    </>
  )
}

export default OrdersPage


