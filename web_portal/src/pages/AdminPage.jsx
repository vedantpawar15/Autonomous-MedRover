import { useState, useEffect, useCallback, useRef } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { supabase } from '../lib/supabaseClient'
import {
  cacheOrders,
  getCachedOrders,
  updateCachedOrderStatus,
  removeCachedOrder,
  getActiveDelivery,
  cacheActiveDelivery,
} from '../lib/deliveryCache'
import { enqueue } from '../lib/syncQueue'


// â”€â”€ Status helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
const STATUS_META = {
  pending    : { bg: '#fef9c3', color: '#854d0e', label: 'Pending'    },
  in_transit : { bg: '#dbeafe', color: '#1e40af', label: 'In Transit' },
  delivered  : { bg: '#dcfce7', color: '#166534', label: 'Delivered'  },
}

function StatusBadge({ status }) {
  const s = STATUS_META[status] || { bg: '#f3f4f6', color: '#374151', label: status }
  return (
    <span className="admin-status-badge" style={{ background: s.bg, color: s.color }}>
      {s.label}
    </span>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// OVERVIEW TAB
// ——————————————————————————————————————————————————————————————————————————————
function OverviewTab() {
  const [stats,   setStats]   = useState({ total: 0, pending: 0, in_transit: 0, delivered: 0, medicines: 0 })
  const [loading, setLoading] = useState(true)

  const fetchStats = useCallback(async () => {
    if (!supabase) return
    setLoading(true)
    try {
      const [ordRes, medRes] = await Promise.all([
        supabase.from('orders').select('status'),
        supabase.from('medicines').select('id', { count: 'exact', head: true }),
      ])
      const o = ordRes.data || []
      setStats({
        total      : o.length,
        pending    : o.filter(x => x.status === 'pending').length,
        in_transit : o.filter(x => x.status === 'in_transit').length,
        delivered  : o.filter(x => x.status === 'delivered').length,
        medicines  : medRes.count || 0,
      })
    } catch (e) { console.error(e) }
    finally { setLoading(false) }
  }, [])

  useEffect(() => { fetchStats() }, [fetchStats])

  const cards = [
    { label: 'Total Orders',   value: stats.total,      icon: 'bi-clipboard2-pulse-fill', iconBg: 'rgba(26,60,74,0.1)',    iconColor: '#1a3c4a'  },
    { label: 'Pending',        value: stats.pending,    icon: 'bi-hourglass-split',       iconBg: 'rgba(234,179,8,0.12)',  iconColor: '#ca8a04'  },
    { label: 'In Transit',     value: stats.in_transit, icon: 'bi-robot',                 iconBg: 'rgba(37,99,235,0.1)',   iconColor: '#2563eb'  },
    { label: 'Delivered',      value: stats.delivered,  icon: 'bi-check2-circle',         iconBg: 'rgba(22,163,74,0.1)',   iconColor: '#16a34a'  },
    { label: 'Medicines',      value: stats.medicines,  icon: 'bi-capsule',               iconBg: 'rgba(139,92,246,0.1)',  iconColor: '#7c3aed'  },
  ]

  return (
    <div>
      <div className="admin-tab-header">
        <div>
          <h4 className="admin-tab-title">Overview</h4>
          <p className="admin-tab-subtitle">Live system statistics from the database</p>
        </div>
        <button className="admin-refresh-btn" onClick={fetchStats}>
          <i className="bi bi-arrow-clockwise" /> Refresh
        </button>
      </div>

      {loading ? (
        <div className="admin-loading">
          <div className="admin-spinner" />
          <span>Loading statistics…</span>
        </div>
      ) : (
        <div className="admin-stats-grid">
          {cards.map(c => (
            <div className="admin-stat-card" key={c.label}>
              <div className="admin-stat-card-top">
                <div className="admin-stat-icon" style={{ background: c.iconBg, color: c.iconColor }}>
                  <i className={`bi ${c.icon}`} />
                </div>
              </div>
              <div className="admin-stat-value">{c.value}</div>
              <div className="admin-stat-label">{c.label}</div>
            </div>
          ))}
        </div>
      )}

      <div className="admin-info-banner mt-4">
        <i className="bi bi-info-circle-fill" style={{ flexShrink: 0 }} />
        <span>
          Use the <strong>Orders</strong> tab to update delivery statuses,
          or <strong>Medicines</strong> to manage the inventory.
          The <strong>Robot</strong> tab shows live delivery activity.
        </span>
      </div>
    </div>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// ORDERS TAB
// ——————————————————————————————————————————————————————————————————————————————
function OrdersTab() {
  const [orders,     setOrders]     = useState(getCachedOrders)
  const [loading,    setLoading]    = useState(false)
  const [error,      setError]      = useState('')
  const [filter,     setFilter]     = useState('all')
  const [search,     setSearch]     = useState('')
  const [updatingId, setUpdatingId] = useState(null)
  const [deletingId, setDeletingId] = useState(null)

  const fetchOrders = useCallback(async () => {
    // 1. Instant load from local cache memory
    const cached = getCachedOrders()
    if (cached && cached.length > 0) {
      setOrders(cached)
    }

    if (!supabase || !navigator.onLine) {
      return
    }

    setLoading(true); setError('')
    try {
      const { data, error: err } = await supabase
        .from('orders').select('*').order('created_at', { ascending: false })
      if (err) {
        if (!cached || cached.length === 0) {
          setError('Could not load orders from database.')
        }
        return
      }
      setOrders(data || [])
      cacheOrders(data || [])
    } catch {
      if (!cached || cached.length === 0) {
        setError('Unexpected network error. Displaying cached orders if available.')
      }
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchOrders() }, [fetchOrders])

  const handleStatusChange = async (id, newStatus) => {
    setUpdatingId(id)

    // 1. Immediately update cache memory & optimistic UI
    const updated = updateCachedOrderStatus(id, newStatus)
    setOrders(updated)

    // 2. If offline or no supabase, enqueue for background sync
    if (!supabase || !navigator.onLine) {
      enqueue('UPDATE_ORDER_STATUS', { orderId: id, status: newStatus })
      setUpdatingId(null)
      return
    }

    try {
      const { error: err } = await supabase.from('orders').update({ status: newStatus }).eq('id', id)
      if (err) {
        // Enqueue if database returned error or dropped connection
        enqueue('UPDATE_ORDER_STATUS', { orderId: id, status: newStatus })
      }
    } catch {
      enqueue('UPDATE_ORDER_STATUS', { orderId: id, status: newStatus })
    } finally {
      setUpdatingId(null)
    }
  }

  const handleDelete = async (order) => {
    if (!window.confirm(`Delete Order #${order.id}? This is permanent.`)) return
    setDeletingId(order.id)

    // 1. Update cache memory & optimistic UI
    removeCachedOrder(order.id)
    setOrders(c => c.filter(o => o.id !== order.id))

    // 2. If offline, enqueue
    if (!supabase || !navigator.onLine) {
      enqueue('DELETE_ORDER', { orderId: order.id })
      setDeletingId(null)
      return
    }

    try {
      await supabase.from('order_items').delete().eq('order_id', order.id)
      const { error: err } = await supabase.from('orders').delete().eq('id', order.id)
      if (err) {
        enqueue('DELETE_ORDER', { orderId: order.id })
      }
    } catch {
      enqueue('DELETE_ORDER', { orderId: order.id })
    } finally {
      setDeletingId(null)
    }
  }

  const filtered = orders.filter(o => {
    const ms = filter === 'all' || o.status === filter
    const mq = !search || String(o.id).includes(search) ||
                (o.room_code || '').toLowerCase().includes(search.toLowerCase())
    return ms && mq
  })

  const fmt = (d) => new Date(d).toLocaleString('en-IN', {
    day: '2-digit', month: 'short', year: 'numeric',
    hour: '2-digit', minute: '2-digit',
  })

  return (
    <div>
      <div className="admin-tab-header">
        <div>
          <h4 className="admin-tab-title">Orders</h4>
          <p className="admin-tab-subtitle">{orders.length} total orders · update status or remove records</p>
        </div>
        <button className="admin-refresh-btn" onClick={fetchOrders}>
          <i className="bi bi-arrow-clockwise" /> Refresh
        </button>
      </div>

      <div className="admin-filter-row">
        <input
          type="text"
          className="admin-search-input"
          placeholder="Search order ID or room…"
          value={search}
          onChange={e => setSearch(e.target.value)}
        />
        <div className="admin-filter-pills">
          {[
            { k: 'all',        l: 'All'        },
            { k: 'pending',    l: 'Pending'    },
            { k: 'in_transit', l: 'In Transit' },
            { k: 'delivered',  l: 'Delivered'  },
          ].map(f => (
            <button key={f.k} className={`admin-filter-pill${filter === f.k ? ' active' : ''}`}
              onClick={() => setFilter(f.k)}>
              {f.l}
            </button>
          ))}
        </div>
        <span className="admin-count-label">{filtered.length} result{filtered.length !== 1 ? 's' : ''}</span>
      </div>

      {loading && <div className="admin-loading"><div className="admin-spinner" /><span>Loading orders…</span></div>}
      {error   && <div className="admin-error-msg"><i className="bi bi-exclamation-triangle-fill" /> {error}</div>}

      {!loading && !error && (
        <div className="admin-table-wrapper">
          <table className="admin-table">
            <thead>
              <tr>
                <th>Order</th>
                <th>Room</th>
                <th>Ward</th>
                <th>Placed</th>
                <th>Status</th>
                <th>Change Status</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              {filtered.length === 0 ? (
                <tr>
                  <td colSpan={7} className="admin-table-empty">
                    <i className="bi bi-inbox me-2" />No orders match this filter.
                  </td>
                </tr>
              ) : filtered.map(order => (
                <tr key={order.id} className={deletingId === order.id ? 'admin-row-deleting' : ''}>
                  <td><span className="admin-order-id">#{order.id}</span></td>
                  <td><strong>Room {order.room_code}</strong></td>
                  <td className="admin-date-cell">{order.room_label || '—'}</td>
                  <td className="admin-date-cell">{fmt(order.created_at)}</td>
                  <td><StatusBadge status={order.status} /></td>
                  <td>
                    <select
                      className="admin-status-select"
                      value={order.status}
                      disabled={updatingId === order.id}
                      onChange={e => handleStatusChange(order.id, e.target.value)}
                    >
                      <option value="pending">Pending</option>
                      <option value="in_transit">In Transit</option>
                      <option value="delivered">Delivered</option>
                    </select>
                  </td>
                  <td>
                    <div className="admin-action-btns">
                      <button
                        className="admin-delete-btn"
                        onClick={() => handleDelete(order)}
                        disabled={deletingId === order.id}
                        title="Delete order"
                      >
                        <i className="bi bi-trash3" />
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// MEDICINES TAB
// ——————————————————————————————————————————————————————————————————————————————
const EMPTY = { name: '', brand: '', pack_info: '', image_url: '', mrp: '', requires_rx: false }

function MedicinesTab() {
  const [medicines,  setMedicines]  = useState([])
  const [loading,    setLoading]    = useState(true)
  const [error,      setError]      = useState('')
  const [search,     setSearch]     = useState('')
  const [showModal,  setShowModal]  = useState(false)
  const [editMed,    setEditMed]    = useState(null)
  const [form,       setForm]       = useState(EMPTY)
  const [saving,     setSaving]     = useState(false)
  const [deletingId, setDeletingId] = useState(null)

  const fetchMedicines = useCallback(async () => {
    if (!supabase) return
    setLoading(true); setError('')
    try {
      const { data, error: err } = await supabase
        .from('medicines').select('*').order('name', { ascending: true })
      if (err) { setError('Could not load medicines.'); return }
      setMedicines(data || [])
    } catch { setError('Unexpected error.') }
    finally { setLoading(false) }
  }, [])

  useEffect(() => { fetchMedicines() }, [fetchMedicines])

  const openAdd  = () => { setEditMed(null); setForm(EMPTY); setShowModal(true) }
  const openEdit = (m) => {
    setEditMed(m)
    setForm({ name: m.name||'', brand: m.brand||'', pack_info: m.pack_info||'',
              image_url: m.image_url||'', mrp: m.mrp??'', requires_rx: !!m.requires_rx })
    setShowModal(true)
  }

  const handleSave = async (e) => {
    e.preventDefault()
    if (!supabase) return
    setSaving(true)
    const payload = {
      name: form.name.trim(), brand: form.brand.trim(),
      pack_info: form.pack_info.trim(),
      image_url: form.image_url.trim() || null,
      mrp: parseFloat(form.mrp) || 0,
      requires_rx: form.requires_rx,
    }
    try {
      const { error: err } = editMed
        ? await supabase.from('medicines').update(payload).eq('id', editMed.id)
        : await supabase.from('medicines').insert(payload)
      if (err) { alert('Save failed.'); return }
      setShowModal(false); fetchMedicines()
    } catch { alert('Error.') }
    finally { setSaving(false) }
  }

  const handleDelete = async (med) => {
    if (!window.confirm(`Delete "${med.name}"? This is permanent.`)) return
    if (!supabase) return
    const prev = medicines
    setDeletingId(med.id)
    setMedicines(c => c.filter(m => m.id !== med.id))
    try {
      const { error: err } = await supabase.from('medicines').delete().eq('id', med.id)
      if (err) { setMedicines(prev); alert('Delete failed.') }
    } catch { setMedicines(prev) }
    finally { setDeletingId(null) }
  }

  const filtered = medicines.filter(m =>
    m.name?.toLowerCase().includes(search.toLowerCase()) ||
    m.brand?.toLowerCase().includes(search.toLowerCase())
  )

  return (
    <div>
      <div className="admin-tab-header">
        <div>
          <h4 className="admin-tab-title">Medicines</h4>
          <p className="admin-tab-subtitle">{medicines.length} items in inventory</p>
        </div>
        <div className="d-flex gap-2">
          <button className="admin-refresh-btn" onClick={fetchMedicines}>
            <i className="bi bi-arrow-clockwise" /> Refresh
          </button>
          <button className="admin-add-btn" onClick={openAdd}>
            <i className="bi bi-plus-lg" /> Add Medicine
          </button>
        </div>
      </div>

      <div className="admin-filter-row">
        <input type="text" className="admin-search-input"
          placeholder="Search by name or brand…"
          value={search} onChange={e => setSearch(e.target.value)} />
        <span className="admin-count-label">{filtered.length} of {medicines.length}</span>
      </div>

      {loading && <div className="admin-loading"><div className="admin-spinner" /><span>Loading medicines…</span></div>}
      {error   && <div className="admin-error-msg"><i className="bi bi-exclamation-triangle-fill" /> {error}</div>}

      {!loading && !error && (
        <div className="admin-table-wrapper">
          <table className="admin-table">
            <thead>
              <tr>
                <th>Name</th>
                <th>Brand</th>
                <th>Pack</th>
                <th>MRP</th>
                <th>Rx</th>
                <th style={{width:80}}>Actions</th>
              </tr>
            </thead>
            <tbody>
              {filtered.length === 0 ? (
                <tr><td colSpan={6} className="admin-table-empty">
                  <i className="bi bi-inbox me-2" />No medicines found.
                </td></tr>
              ) : filtered.map(m => (
                <tr key={m.id} className={deletingId === m.id ? 'admin-row-deleting' : ''}>
                  <td><strong>{m.name}</strong></td>
                  <td style={{color:'#6b7c8a'}}>{m.brand || '—'}</td>
                  <td style={{color:'#6b7c8a',fontSize:'0.82rem'}}>{m.pack_info || '—'}</td>
                  <td><strong>₹{Number(m.mrp||0).toFixed(2)}</strong></td>
                  <td>
                    {m.requires_rx
                      ? <span className="admin-rx-yes"><i className="bi bi-check-circle-fill me-1"/>Yes</span>
                      : <span className="admin-rx-no">No</span>}
                  </td>
                  <td>
                    <div className="admin-action-btns">
                      <button className="admin-edit-btn" onClick={() => openEdit(m)} title="Edit">
                        <i className="bi bi-pencil" />
                      </button>
                      <button className="admin-delete-btn"
                        onClick={() => handleDelete(m)} disabled={deletingId === m.id} title="Delete">
                        <i className="bi bi-trash3" />
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Modal */}
      {showModal && (
        <div className="admin-modal-overlay" onClick={() => setShowModal(false)}>
          <div className="admin-modal" onClick={e => e.stopPropagation()}>
            <div className="admin-modal-header">
              <h5>
                <i className={`bi ${editMed ? 'bi-pencil-square' : 'bi-plus-circle-fill'} me-2`}
                  style={{color:'#1E7F78'}} />
                {editMed ? 'Edit Medicine' : 'Add Medicine'}
              </h5>
              <button className="admin-modal-close" onClick={() => setShowModal(false)}>
                <i className="bi bi-x-lg" />
              </button>
            </div>
            <form onSubmit={handleSave} className="admin-modal-body">
              <div className="admin-form-grid">
                <div className="admin-form-group">
                  <label>Name *</label>
                  <input type="text" required placeholder="e.g. Paracetamol 500mg"
                    value={form.name} onChange={e => setForm(f=>({...f,name:e.target.value}))} />
                </div>
                <div className="admin-form-group">
                  <label>Brand *</label>
                  <input type="text" required placeholder="e.g. Cipla"
                    value={form.brand} onChange={e => setForm(f=>({...f,brand:e.target.value}))} />
                </div>
                <div className="admin-form-group">
                  <label>Pack Info</label>
                  <input type="text" placeholder="e.g. 10 tablets / strip"
                    value={form.pack_info} onChange={e => setForm(f=>({...f,pack_info:e.target.value}))} />
                </div>
                <div className="admin-form-group">
                  <label>MRP (₹) *</label>
                  <input type="number" required min="0" step="0.01" placeholder="25.50"
                    value={form.mrp} onChange={e => setForm(f=>({...f,mrp:e.target.value}))} />
                </div>
                <div className="admin-form-group" style={{gridColumn:'1 / -1'}}>
                  <label>Image URL (optional)</label>
                  <input type="url" placeholder="https://…"
                    value={form.image_url} onChange={e => setForm(f=>({...f,image_url:e.target.value}))} />
                </div>
                <div className="admin-form-group admin-checkbox-group" style={{gridColumn:'1 / -1'}}>
                  <label className="admin-checkbox-label">
                    <input type="checkbox" checked={form.requires_rx}
                      onChange={e => setForm(f=>({...f,requires_rx:e.target.checked}))} />
                    Requires Prescription (Rx)
                    <span style={{fontWeight:400,color:'#8a9bae',marginLeft:6,fontSize:'0.82rem'}}>
                      — customer must show valid prescription
                    </span>
                  </label>
                </div>
              </div>
              <div className="admin-modal-footer">
                <button type="button" className="admin-cancel-btn" onClick={() => setShowModal(false)}>
                  Cancel
                </button>
                <button type="submit" className="admin-save-btn" disabled={saving}>
                  {saving
                    ? <><div className="admin-spinner-sm" />Saving…</>
                    : <><i className="bi bi-check-lg" />{editMed ? 'Update' : 'Add Medicine'}</>}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// ROBOT TAB
// ——————————————————————————————————————————————————————————————————————————————
function RobotTab() {
  const [active,  setActive]  = useState(getActiveDelivery)
  const [recent,  setRecent]  = useState(() => getCachedOrders().slice(0, 8))
  const [loading, setLoading] = useState(false)

  const fetchStatus = useCallback(async () => {
    // 1. Check local delivery cache memory first
    const cachedActive = getActiveDelivery()
    if (cachedActive) setActive(cachedActive)
    const cachedOrders = getCachedOrders()
    if (cachedOrders && cachedOrders.length > 0) {
      setRecent(cachedOrders.slice(0, 8))
    }

    if (!supabase || !navigator.onLine) {
      return
    }

    setLoading(true)
    try {
      const [aRes, rRes] = await Promise.all([
        supabase.from('orders').select('*').eq('status','in_transit')
          .order('created_at',{ascending:false}).limit(1),
        supabase.from('orders').select('*')
          .order('created_at',{ascending:false}).limit(8),
      ])
      const activeOrd = aRes.data?.[0] || null
      setActive(activeOrd)
      if (activeOrd) {
        cacheActiveDelivery(activeOrd)
      }
      setRecent(rRes.data || [])
    } catch(e) {
      console.error(e)
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchStatus() }, [fetchStatus])

  const fmt = (d) => new Date(d).toLocaleString('en-IN', {
    day: '2-digit', month: 'short', year: 'numeric',
    hour: '2-digit', minute: '2-digit',
  })

  const isActive = !!active

  return (
    <div>
      <div className="admin-tab-header">
        <div>
          <h4 className="admin-tab-title">Robot Status</h4>
          <p className="admin-tab-subtitle">Live MedRover delivery tracking</p>
        </div>
        <button className="admin-refresh-btn" onClick={fetchStatus}>
          <i className="bi bi-arrow-clockwise" /> Refresh
        </button>
      </div>

      {loading ? (
        <div className="admin-loading"><div className="admin-spinner" /><span>Checking robot statusâ€¦</span></div>
      ) : (
        <>
          <div className={`robot-status-card ${isActive ? 'active' : 'idle'} mb-4`}>
            <div className="robot-status-icon-wrap">
              <i className="bi bi-robot" />
              {isActive && <span className="robot-pulse-ring" />}
            </div>
            <div className="robot-status-info">
              <h5 className="robot-status-label">
                <span className={`robot-status-dot ${isActive ? 'green' : 'grey'}`} />
                {isActive ? 'MedRover is delivering now' : 'MedRover is idle'}
              </h5>
              {isActive ? (
                <div className="robot-active-details">
                  <p>
                    Delivering <strong>Order #{active.id}</strong> to{' '}
                    <strong>Room {active.room_code}</strong>
                    {active.room_label && ` Â· ${active.room_label}`}
                  </p>
                  <p style={{marginTop:4,fontSize:'0.8rem',color:'#8a9bae'}}>
                    Dispatched {fmt(active.created_at)}
                  </p>
                </div>
              ) : (
                <p style={{color:'#8a9bae',fontSize:'0.88rem',margin:0}}>
                  No active delivery. Robot is docked and ready.
                </p>
              )}
            </div>
          </div>

          {recent.length > 0 && (
            <>
              <h6 className="admin-section-subtitle mb-3">Recent Deliveries</h6>
              <div className="robot-timeline">
                {recent.map((o, idx) => (
                  <div className="robot-timeline-item" key={o.id}>
                    <div className={`robot-timeline-dot ${o.status}`} />
                    {idx < recent.length-1 && <div className="robot-timeline-line" />}
                    <div className="robot-timeline-content">
                      <div className="robot-timeline-title">
                        Order #{o.id} â†’ Room {o.room_code}
                        <StatusBadge status={o.status} />
                      </div>
                      <div className="robot-timeline-date">
                        {fmt(o.created_at)}
                        {o.room_label && <span style={{marginLeft:8,color:'#b0bac5'}}>Â· {o.room_label}</span>}
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </>
          )}
        </>
      )}
    </div>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// USERS TAB
// ——————————————————————————————————————————————————————————————————————————————
const USER_STATUS_META = {
  pending  : { bg: '#fef9c3', color: '#854d0e', label: 'Pending'  },
  approved : { bg: '#dcfce7', color: '#166534', label: 'Approved' },
  rejected : { bg: '#fee2e2', color: '#991b1b', label: 'Rejected' },
}

function UserStatusBadge({ status }) {
  const s = USER_STATUS_META[status] || { bg: '#f3f4f6', color: '#374151', label: status }
  return (
    <span className="admin-status-badge" style={{ background: s.bg, color: s.color }}>
      {s.label}
    </span>
  )
}

function UsersTab() {
  const [users,      setUsers]      = useState([])
  const [loading,    setLoading]    = useState(true)
  const [error,      setError]      = useState('')
  const [filter,     setFilter]     = useState('all')
  const [actionId,   setActionId]   = useState(null)

  const fetchUsers = useCallback(async () => {
    if (!supabase) return
    setLoading(true); setError('')
    try {
      // Use SECURITY DEFINER RPC to bypass RLS (admin has no Supabase session)
      const { data, error: err } = await supabase.rpc('admin_get_all_users')
      if (err) { setError('Could not load users: ' + err.message); return }
      setUsers(data || [])
    } catch { setError('Unexpected network error.') }
    finally { setLoading(false) }
  }, [])

  useEffect(() => { fetchUsers() }, [fetchUsers])

  const setStatus = async (id, status) => {
    if (!supabase) return
    setActionId(id)
    try {
      // Use SECURITY DEFINER RPC to bypass RLS for update
      const { error: err } = await supabase.rpc('admin_set_user_status', {
        target_id: id,
        new_status: status,
      })
      if (!err) setUsers(p => p.map(u => u.id === id ? { ...u, approval_status: status } : u))
      else alert('Action failed: ' + err.message)
    } catch { alert('Error.') }
    finally { setActionId(null) }
  }

  const filtered = filter === 'all' ? users : users.filter(u => u.approval_status === filter)

  const pendingCount = users.filter(u => u.approval_status === 'pending').length

  const fmt = (d) => new Date(d).toLocaleString('en-IN', {
    day: '2-digit', month: 'short', year: 'numeric',
    hour: '2-digit', minute: '2-digit',
  })

  return (
    <div>
      <div className="admin-tab-header">
        <div>
          <h4 className="admin-tab-title">User Approvals</h4>
          <p className="admin-tab-subtitle">
            {users.length} registered user{users.length !== 1 ? 's' : ''}
            {pendingCount > 0 && (
              <span style={{ marginLeft: 8, background: '#fef9c3', color: '#854d0e',
                padding: '1px 8px', borderRadius: 99, fontSize: '0.78rem', fontWeight: 600 }}>
                {pendingCount} pending
              </span>
            )}
          </p>
        </div>
        <button className="admin-refresh-btn" onClick={fetchUsers}>
          <i className="bi bi-arrow-clockwise" /> Refresh
        </button>
      </div>

      <div className="admin-filter-row">
        <div className="admin-filter-pills">
          {[
            { k: 'all',      l: 'All'      },
            { k: 'pending',  l: 'Pending'  },
            { k: 'approved', l: 'Approved' },
            { k: 'rejected', l: 'Rejected' },
          ].map(f => (
            <button key={f.k}
              className={`admin-filter-pill${filter === f.k ? ' active' : ''}`}
              onClick={() => setFilter(f.k)}>
              {f.l}
              {f.k === 'pending' && pendingCount > 0 && (
                <span style={{ marginLeft: 5, background: '#ef4444', color: '#fff',
                  borderRadius: 99, padding: '0 5px', fontSize: '0.7rem', fontWeight: 700 }}>
                  {pendingCount}
                </span>
              )}
            </button>
          ))}
        </div>
        <span className="admin-count-label">{filtered.length} result{filtered.length !== 1 ? 's' : ''}</span>
      </div>

      {loading && <div className="admin-loading"><div className="admin-spinner" /><span>Loading users…</span></div>}
      {error   && <div className="admin-error-msg"><i className="bi bi-exclamation-triangle-fill" /> {error}</div>}

      {!loading && !error && (
        <div className="admin-table-wrapper">
          <table className="admin-table">
            <thead>
              <tr>
                <th>Name</th>
                <th>Email</th>
                <th>Status</th>
                <th>Registered</th>
                <th>Actions</th>
              </tr>
            </thead>
            <tbody>
              {filtered.length === 0 ? (
                <tr><td colSpan={5} className="admin-table-empty">
                  <i className="bi bi-inbox me-2" />No users in this category.
                </td></tr>
              ) : filtered.map(u => (
                 <tr key={u.id}>
                  <td><strong>{u.full_name || <span style={{color:'#aab'}}>—</span>}</strong></td>
                  <td style={{ color: '#4b6070' }}>{u.email}</td>
                  <td><UserStatusBadge status={u.approval_status} /></td>
                  <td className="admin-date-cell">{fmt(u.created_at)}</td>
                  <td style={{ minWidth: 190 }}>
                    <div className="admin-action-btns">
                      {u.approval_status !== 'approved' && (
                        <button
                          className="user-action-approve"
                          disabled={actionId === u.id}
                          onClick={() => setStatus(u.id, 'approved')}
                          title="Approve user"
                        >
                          <i className="bi bi-check-lg" /> Approve
                        </button>
                      )}
                      {u.approval_status !== 'rejected' && (
                        <button
                          className="user-action-reject"
                          disabled={actionId === u.id}
                          onClick={() => setStatus(u.id, 'rejected')}
                          title="Reject user"
                        >
                          <i className="bi bi-x-lg" /> Reject
                        </button>
                      )}
                      {u.approval_status === 'approved' && (
                        <button
                          className="user-action-revoke"
                          disabled={actionId === u.id}
                          onClick={() => setStatus(u.id, 'pending')}
                          title="Revoke access"
                        >
                          <i className="bi bi-slash-circle" /> Revoke
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      <div className="admin-info-banner mt-4">
        <i className="bi bi-info-circle-fill" style={{ flexShrink: 0 }} />
        <span>
          <strong>Approve</strong> a user to grant portal access.
          <strong> Reject</strong> to deny access.
          <strong> Revoke</strong> to suspend an approved user.
          Users cannot log in until approved.
        </span>
      </div>
    </div>
  )
}

// ——————————————————————————————————————————————————————————————————————————————
// ROOT — Admin Page
// ——————————————————————————————————————————————————————————————————————————————
const TABS = [
  { id: 'overview',  label: 'Overview',     icon: 'bi-grid-1x2'              },
  { id: 'orders',    label: 'Orders',        icon: 'bi-clipboard2-pulse'      },
  { id: 'medicines', label: 'Medicines',     icon: 'bi-capsule'               },
  { id: 'robot',     label: 'Robot Status',  icon: 'bi-robot'                 },
  { id: 'users',     label: 'User Approvals', icon: 'bi-people'               },
]

// ──────────────────────────────────────────────────────────────────────────────
// ADMIN LOGIN GATE
// Completely separate from the Supabase login page.
// Credentials: email = "admin18", password = "admin@1234"
// ──────────────────────────────────────────────────────────────────────────────
const ADMIN_ID  = 'admin18'
const ADMIN_PWD = 'admin@1234'
const SESSION_KEY = 'medrover_admin_auth'

function AdminLoginGate() {
  const [authed,   setAuthed]   = useState(() => sessionStorage.getItem(SESSION_KEY) === 'true')
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [showPass, setShowPass] = useState(false)
  const [error,    setError]    = useState('')
  const [loading,  setLoading]  = useState(false)
  const navigate = useNavigate()

  const handleLogin = (e) => {
    e.preventDefault()
    setError('')
    setLoading(true)
    // Simulate a brief delay for UX
    setTimeout(() => {
      if (username.trim() === ADMIN_ID && password === ADMIN_PWD) {
        sessionStorage.setItem(SESSION_KEY, 'true')
        setAuthed(true)
      } else {
        setError('Invalid admin credentials. Please try again.')
      }
      setLoading(false)
    }, 500)
  }

  const handleLogout = () => {
    sessionStorage.removeItem(SESSION_KEY)
    setAuthed(false)
    setUsername('')
    setPassword('')
  }

  if (authed) return <AdminDashboard onLock={handleLogout} />

  return (
    <div className="admin-gate-page">
      <div className="admin-gate-card">

        {/* Header */}
        <div className="admin-gate-header">
          <div className="admin-gate-icon">
            <i className="bi bi-shield-lock-fill" />
          </div>
          <h2 className="admin-gate-title">Admin Access</h2>
          <p className="admin-gate-sub">
            This area is restricted to authorised administrators only.
          </p>
        </div>

        {/* Form */}
        <form onSubmit={handleLogin} className="admin-gate-form" noValidate>

          <div className="admin-gate-field">
            <label htmlFor="adminUser">Admin Username</label>
            <div className={`admin-gate-input-wrap${error ? ' error' : ''}`}>
              <i className="bi bi-person-badge" />
              <input
                id="adminUser"
                type="text"
                placeholder="Enter admin username"
                autoComplete="username"
                value={username}
                onChange={e => { setUsername(e.target.value); setError('') }}
                required
              />
            </div>
          </div>

          <div className="admin-gate-field">
            <label htmlFor="adminPass">Password</label>
            <div className={`admin-gate-input-wrap${error ? ' error' : ''}`}>
              <i className="bi bi-key" />
              <input
                id="adminPass"
                type={showPass ? 'text' : 'password'}
                placeholder="Enter admin password"
                autoComplete="current-password"
                value={password}
                onChange={e => { setPassword(e.target.value); setError('') }}
                required
              />
              <button
                type="button"
                className="admin-gate-eye"
                onClick={() => setShowPass(v => !v)}
                tabIndex={-1}
                aria-label={showPass ? 'Hide' : 'Show'}
              >
                <i className={`bi ${showPass ? 'bi-eye-slash' : 'bi-eye'}`} />
              </button>
            </div>
          </div>

          {error && (
            <div className="admin-gate-error">
              <i className="bi bi-exclamation-circle-fill" />
              {error}
            </div>
          )}

          <button
            type="submit"
            className="admin-gate-submit"
            disabled={loading || !username || !password}
          >
            {loading
              ? <><span className="admin-gate-spinner" /> Verifying…</>
              : <><i className="bi bi-unlock-fill" /> Access Dashboard</>
            }
          </button>
        </form>

        {/* Back link */}
        <button className="admin-gate-back" onClick={() => navigate('/')}>
          <i className="bi bi-arrow-left" /> Back to portal
        </button>

        <p className="admin-gate-note">
          <i className="bi bi-info-circle me-1" />
          This is separate from the staff login page.
        </p>
      </div>
    </div>
  )
}

// ──────────────────────────────────────────────────────────────────────────────
// ADMIN DASHBOARD (shown after successful admin login)
// ──────────────────────────────────────────────────────────────────────────────
function AdminDashboard({ onLock }) {
  const [activeTab,   setActiveTab]   = useState('overview')
  const [sidebarOpen, setSidebarOpen] = useState(false)
  const [time,        setTime]        = useState(new Date())

  useEffect(() => {
    const t = setInterval(() => setTime(new Date()), 30_000)
    return () => clearInterval(t)
  }, [])

  const tab = TABS.find(t => t.id === activeTab)

  return (
    <div className="admin-layout">

      {/* Sidebar */}
      <aside className={`admin-sidebar${sidebarOpen ? ' open' : ''}`}>
        <div className="admin-sidebar-brand">
          <img src="/assets/logo/white.png" alt="MedRover" />
          <span>Admin</span>
        </div>

        <nav className="admin-sidebar-nav">
          <div className="admin-nav-section-label">Management</div>
          {TABS.map(t => (
            <button
              key={t.id}
              className={`admin-nav-item${activeTab === t.id ? ' active' : ''}`}
              onClick={() => { setActiveTab(t.id); setSidebarOpen(false) }}
            >
              <i className={`bi ${t.icon}`} />
              <span>{t.label}</span>
            </button>
          ))}
        </nav>

        <div className="admin-sidebar-footer">
          <div className="admin-nav-section-label">Account</div>
          <Link to="/" className="admin-nav-item">
            <i className="bi bi-arrow-left-circle" />
            <span>Back to Portal</span>
          </Link>
          <button className="admin-nav-item admin-lock-btn" onClick={onLock}>
            <i className="bi bi-lock" />
            <span>Lock Dashboard</span>
          </button>
        </div>
      </aside>

      {sidebarOpen && (
        <div className="admin-sidebar-overlay" onClick={() => setSidebarOpen(false)} />
      )}

      {/* Main */}
      <main className="admin-main">
        {/* Topbar */}
        <div className="admin-topbar">
          <button className="admin-menu-btn d-lg-none" onClick={() => setSidebarOpen(v => !v)}>
            <i className="bi bi-list" />
          </button>

          <div className="admin-topbar-breadcrumb">
            <span>Admin</span>
            <span className="sep">/</span>
            <span className="current">{tab?.label}</span>
          </div>

          <div className="admin-topbar-right">
            <span className="admin-topbar-time">
              {time.toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })}
            </span>
            <span className="admin-topbar-badge">
              <i className="bi bi-shield-check" />
              Admin
            </span>
          </div>
        </div>

        {/* Content */}
        <div className="admin-content">
          {activeTab === 'overview'  && <OverviewTab />}
          {activeTab === 'orders'    && <OrdersTab />}
          {activeTab === 'medicines' && <MedicinesTab />}
          {activeTab === 'robot'     && <RobotTab />}
          {activeTab === 'users'     && <UsersTab />}
        </div>
      </main>
    </div>
  )
}

export default AdminLoginGate


