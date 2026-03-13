import { useEffect, useState } from 'react'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'

function OrdersPage() {
  const [orders, setOrders] = useState([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [cartCount, setCartCount] = useState(0)

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

  // Keep cart badge in sync using localStorage
  useEffect(() => {
    try {
      const raw = window.localStorage.getItem('medrover_cart')
      if (!raw) {
        setCartCount(0)
        return
      }
      const parsed = JSON.parse(raw)
      if (!Array.isArray(parsed)) {
        setCartCount(0)
        return
      }
      const count = parsed.reduce(
        (sum, item) => sum + (item.selectedQty || 1),
        0
      )
      setCartCount(count)
    } catch (e) {
      console.error('Failed to load cart count on OrdersPage', e)
      setCartCount(0)
    }
  }, [])

  return (
    <>
      <Navbar variant="inner" cartCount={cartCount} />

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
                <div className="order-summary-item d-flex justify-content-between" key={order.id}>
                  <div>
                    <div className="fw-semibold">
                      Order #{order.id} &middot; Room {order.room_code}
                    </div>
                    <div className="small text-muted">
                      {order.room_label || 'No label'} &middot;{' '}
                      {new Date(order.created_at).toLocaleString()}
                    </div>
                  </div>
                  <span className="badge bg-success-subtle text-success">
                    {order.status}
                  </span>
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


