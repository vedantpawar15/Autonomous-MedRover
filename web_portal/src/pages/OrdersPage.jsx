import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'
import { cartTotalQty, readCartLines } from '../lib/cartStorage'
import { useAuth } from '../contexts/AuthContext'
import { cacheUserOrders, getCachedUserOrders, cacheActiveDelivery, saveOfflineOrder } from '../lib/deliveryCache'
import { enqueue } from '../lib/syncQueue'

function OrdersPage() {
  const { user } = useAuth()
  const navigate = useNavigate()
  const [orders, setOrders] = useState(() => getCachedUserOrders(user?.id))
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [cartCount, setCartCount] = useState(0)
  const [deletingId, setDeletingId] = useState(null)
  const [repeatingId, setRepeatingId] = useState(null)
  const [repeatModal, setRepeatModal] = useState(null)
  const [deleteModal, setDeleteModal] = useState(null)

  useEffect(() => {
    const cached = getCachedUserOrders(user?.id)
    if (cached && cached.length > 0) setOrders(cached)
    if (!supabase || !user?.id || !navigator.onLine) return

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
          if (!cached || cached.length === 0) setError('Could not load orders from server.')
        } else {
          setOrders(data || [])
          cacheUserOrders(user?.id, data || [])
        }
      } catch (e) {
        if (!cached || cached.length === 0) setError('Network disconnected. Displaying cached orders.')
      } finally {
        setLoading(false)
      }
    }
    fetchOrders()
  }, [user?.id])

  const openDeleteModal = (order) => setDeleteModal(order)

  const confirmDelete = async () => {
    if (!deleteModal) return
    const order = deleteModal
    setDeleteModal(null)
    if (!supabase) { alert('Supabase is not configured. Please check your .env.'); return }
    setDeletingId(order.id)
    setError('')
    const remaining = orders.filter((o) => o.id !== order.id)
    setOrders(remaining)
    cacheUserOrders(user?.id, remaining)
    if (!navigator.onLine) { enqueue('DELETE_ORDER', { orderId: order.id }); setDeletingId(null); return }
    try {
      const { error: itemsErr } = await supabase.from('order_items').delete().eq('order_id', order.id)
      if (itemsErr) throw itemsErr
      const { error: orderErr } = await supabase.from('orders').delete().eq('id', order.id)
      if (orderErr) throw orderErr
    } catch (e) {
      enqueue('DELETE_ORDER', { orderId: order.id })
    } finally {
      setDeletingId(null)
    }
  }

  const openRepeatModal = async (order) => {
    if (!supabase) { alert('Supabase is not configured.'); return }
    setRepeatingId(order.id)
    try {
      const { data: items, error: itemsErr } = await supabase
        .from('order_items').select('*').eq('order_id', order.id)
      if (itemsErr) throw itemsErr
      if (!items || items.length === 0) { alert('No items found for this order.'); return }

      // Fetch medicine metadata (name, pack_info, mrp, image) from medicines table
      const medIds = items.map((it) => it.medicine_id).filter(Boolean)
      const medsMap = {}
      if (medIds.length > 0) {
        try {
          const { data: meds } = await supabase
            .from('medicines')
            .select('*')
            .in('id', medIds)
          if (meds) {
            meds.forEach((m) => { medsMap[m.id] = m })
          }
        } catch (e) {
          console.warn('Could not load extra medicine details', e)
        }
      }

      const enrichedItems = items.map((it) => {
        const med = medsMap[it.medicine_id] || {}
        return {
          ...it,
          medicine_name: med.name || it.medicine_name || it.name || `Medicine #${it.medicine_id}`,
          pack_info: med.pack_info || (it.quantity ? `${it.quantity}` : '1'),
          unit_price: it.unit_price ?? it.mrp ?? med.mrp ?? 0,
          image_url: med.image_url || null,
        }
      })

      setRepeatModal({ order, items: enrichedItems })
    } catch (e) {
      console.error(e)
      alert('Could not load order details. Please try again.')
    } finally {
      setRepeatingId(null)
    }
  }

  const confirmRepeat = async () => {
    if (!repeatModal) return
    const { order, items } = repeatModal
    setRepeatModal(null)
    setRepeatingId(order.id)
    const orderPayload = { room_code: order.room_code, room_label: order.room_label, status: 'pending', user_id: user?.id ?? null }
    const itemsPayload = items.map((item) => ({ medicine_id: item.medicine_id, quantity: item.quantity || 1, unit_price: item.unit_price || 0, mrp: item.mrp || 0 }))
    try {
      if (!navigator.onLine) { saveOfflineOrder(orderPayload, itemsPayload); navigate('/order-success'); return }
      const { data: newOrder, error: orderError } = await supabase.from('orders').insert(orderPayload).select('*').single()
      if (orderError || !newOrder) { saveOfflineOrder(orderPayload, itemsPayload); navigate('/order-success'); return }
      cacheActiveDelivery(newOrder)
      await supabase.from('order_items').insert(itemsPayload.map((it) => ({ ...it, order_id: newOrder.id })))
      const updated = [newOrder, ...orders]
      setOrders(updated)
      cacheUserOrders(user?.id, updated)
      navigate('/order-success')
    } catch (e) {
      alert('Could not place repeat order. Please try again.')
    } finally {
      setRepeatingId(null)
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
                    <div className="fw-semibold">Order #{order.id} &middot; Room {order.room_code}</div>
                    <div className="small text-muted">
                      {order.room_label || 'No label'} &middot; {new Date(order.created_at).toLocaleString()}
                    </div>
                  </div>
                  <div className="d-flex align-items-center gap-2">
                    <span className="badge bg-success-subtle text-success">{order.status}</span>
                    <button type="button" className="btn btn-sm btn-outline-success"
                      onClick={() => openRepeatModal(order)}
                      disabled={repeatingId === order.id || deletingId === order.id}
                      title="Repeat this order">
                      {repeatingId === order.id ? 'Loading...' : 'Repeat'}
                    </button>
                    <button type="button" className="btn btn-sm btn-outline-danger"
                      onClick={() => openDeleteModal(order)}
                      disabled={deletingId === order.id || repeatingId === order.id}
                      title="Delete order">
                      {deletingId === order.id ? 'Deleting...' : 'Delete'}
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </section>

      {repeatModal && (
        <div className="ro-backdrop" onClick={() => setRepeatModal(null)}>
          <div className="ro-modal" onClick={(e) => e.stopPropagation()}>
            <div className="ro-modal-header">
              <div className="ro-modal-icon"><i className="bi bi-arrow-repeat" /></div>
              <div className="ro-modal-header-text">
                <h5 className="ro-modal-title">Confirm Repeat Order</h5>
                <p className="ro-modal-sub">Order #{repeatModal.order.id}</p>
              </div>
              <button className="ro-modal-close" onClick={() => setRepeatModal(null)}>
                <i className="bi bi-x-lg" />
              </button>
            </div>

            <div className="ro-section">
              <p className="ro-label"><i className="bi bi-geo-alt-fill me-1" />Delivery Room</p>
              <div className="ro-room-badge">
                <i className="bi bi-door-open-fill me-2" />
                Room {repeatModal.order.room_code}
                {repeatModal.order.room_label && (
                  <span className="ro-room-ward"> &middot; {repeatModal.order.room_label}</span>
                )}
              </div>
            </div>

            <div className="ro-section">
              <p className="ro-label"><i className="bi bi-capsule me-1" />Items in this order</p>
              <div className="ro-items-list">
                {repeatModal.items.map((item, idx) => {
                  const itemPrice = ((item.unit_price || item.mrp || 0) * (item.quantity || 1))
                  return (
                    <div className="ro-item-card" key={idx}>
                      <div className="ro-card-img-wrap">
                        {item.image_url ? (
                          <img
                            src={item.image_url}
                            alt={item.medicine_name}
                            className="ro-card-img"
                            onError={(e) => {
                              e.currentTarget.style.display = 'none'
                              e.currentTarget.parentElement?.querySelector('.ro-card-icon-fallback')?.classList.add('show')
                            }}
                          />
                        ) : null}
                        <i className={`bi bi-capsule ro-card-icon-fallback${item.image_url ? '' : ' show'}`} />
                      </div>
                      <div className="ro-card-content">
                        <h6 className="ro-card-title">{item.medicine_name}</h6>
                        <div className="ro-card-qty-info">{item.pack_info || item.quantity || 1}</div>
                        <div className="ro-card-price">
                          ₹{Number(itemPrice).toFixed(2)}
                        </div>
                      </div>
                    </div>
                  )
                })}
              </div>
            </div>

            <div className="ro-modal-actions">
              <button className="ro-btn-cancel" onClick={() => setRepeatModal(null)}>Cancel</button>
              <button className="ro-btn-confirm" onClick={confirmRepeat}>
                <i className="bi bi-send-fill me-2" />Place Order
              </button>
            </div>
          </div>
        </div>
      )}

      {deleteModal && (
        <div className="ro-backdrop" onClick={() => setDeleteModal(null)}>
          <div className="ro-modal" onClick={(e) => e.stopPropagation()}>
            <div className="ro-modal-header">
              <div className="ro-modal-icon" style={{ background: 'linear-gradient(135deg,#ef4444,#b91c1c)' }}>
                <i className="bi bi-trash3-fill" />
              </div>
              <div className="ro-modal-header-text">
                <h5 className="ro-modal-title">Delete Order</h5>
                <p className="ro-modal-sub">Order #{deleteModal.id} &middot; Room {deleteModal.room_code}</p>
              </div>
              <button className="ro-modal-close" onClick={() => setDeleteModal(null)}>
                <i className="bi bi-x-lg" />
              </button>
            </div>

            <div className="ro-section" style={{ paddingBottom: '4px' }}>
              <div className="ro-delete-warn">
                <i className="bi bi-exclamation-triangle-fill me-2" />
                This order will be permanently removed and cannot be undone.
              </div>
              <div className="ro-room-badge mt-2" style={{ borderColor: '#fca5a5', background: '#fff1f2', color: '#b91c1c' }}>
                <i className="bi bi-door-open-fill me-2" />
                Room {deleteModal.room_code}
                {deleteModal.room_label && (
                  <span style={{ fontWeight: 400, color: '#dc2626' }}> &middot; {deleteModal.room_label}</span>
                )}
              </div>
            </div>

            <div className="ro-modal-actions">
              <button className="ro-btn-cancel" onClick={() => setDeleteModal(null)}>Cancel</button>
              <button
                className="ro-btn-confirm"
                style={{ background: 'linear-gradient(135deg,#ef4444,#b91c1c)', flex: 1 }}
                onClick={confirmDelete}
              >
                <i className="bi bi-trash3-fill me-2" />Delete Order
              </button>
            </div>
          </div>
        </div>
      )}

      <Footer variant="simple" />
    </>
  )
}

export default OrdersPage
