import { useEffect, useState } from 'react'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'
import { cartTotalQty, readCartLines } from '../lib/cartStorage'

function OrdersPage() {
  const [orders, setOrders] = useState([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [cartCount, setCartCount] = useState(0)
  const [deletingId, setDeletingId] = useState(null)

  useEffect(() => {
    const fetchOrders = async () => {
      if (!supabase) return
      setLoading(true)
      setError('')
      try {
        const { data, error: err } = await supabase
          .from('orders')
          .select('*')
          .order('created_at', { ascending: false })
          .limit(20)

        if (err) {
          console.error('Error loading orders', err)
          setError('Could not load orders from server.')
        } else {
          setOrders(data || [])
        }
      } catch (e) {
        console.error('Exception loading orders', e)
        setError('Unexpected error while loading orders.')
      } finally {
        setLoading(false)
      }
    }

    fetchOrders()
  }, [])

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

    // Optimistic UI update
    const prev = orders
    setOrders((cur) => cur.filter((o) => o.id !== order.id))

    try {
      // If you have a separate table for order items, delete them first.
      // If your DB has ON DELETE CASCADE, this is still safe (it will just delete 0 rows if none exist).
      const { error: itemsErr } = await supabase
        .from('order_items')
        .delete()
        .eq('order_id', order.id)

      if (itemsErr) {
        console.error('Error deleting order items', itemsErr)
        throw itemsErr
      }

      const { error: orderErr } = await supabase
        .from('orders')
        .delete()
        .eq('id', order.id)

      if (orderErr) {
        console.error('Error deleting order', orderErr)
        throw orderErr
      }
    } catch (e) {
      setOrders(prev)
      setError('Could not delete order. Please try again.')
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


