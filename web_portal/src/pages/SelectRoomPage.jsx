import { useState, useEffect, useMemo } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'

const rooms = [
  {
    id: 'A',
    name: 'Room A',
    ward: 'Ward A — Ground Floor',
    type: 'General Ward',
    time: '~3 min delivery'
  },
  {
    id: 'B',
    name: 'Room B',
    ward: 'Ward B — First Floor',
    type: 'ICU Ward',
    time: '~5 min delivery'
  },
  {
    id: 'C',
    name: 'Room C',
    ward: 'Ward C — Second Floor',
    type: 'Pediatric Ward',
    time: '~7 min delivery'
  }
]

function SelectRoomPage() {
  const [selectedRoom, setSelectedRoom] = useState('')
  const [orderItems, setOrderItems] = useState([])
  const navigate = useNavigate()

  // Load current cart from localStorage to show in order summary
  useEffect(() => {
    try {
      const raw = window.localStorage.getItem('medrover_cart')
      if (!raw) return
      const parsed = JSON.parse(raw)
      if (Array.isArray(parsed)) {
        const mapped = parsed.map((item) => ({
          medicineId: item.id,
          name: `${item.name} (x${item.selectedQty})`,
          unitMrp: item.originalPrice,
          unitPrice: item.currentPrice,
          qty: item.selectedQty
        }))
        setOrderItems(mapped)
      }
    } catch (e) {
      console.error('Failed to load cart for order summary', e)
    }
  }, [])

  const cartTotals = useMemo(() => {
    return orderItems.reduce(
      (acc, item) => {
        acc.totalMrp += (item.unitMrp || 0) * (item.qty || 1)
        acc.totalAmount += (item.unitPrice || 0) * (item.qty || 1)
        return acc
      },
      { totalMrp: 0, totalAmount: 0 }
    )
  }, [orderItems])

  const savings = cartTotals.totalMrp - cartTotals.totalAmount
  const cartCount = orderItems.reduce((sum, item) => sum + (item.qty || 1), 0)

  const handleNavSearch = (query) => {
    navigate(`/search?query=${encodeURIComponent(query)}`)
  }

  const handlePlaceOrder = async () => {
    if (!selectedRoom) {
      alert('Please select a delivery room.')
      return
    }

    if (!supabase) {
      alert('Supabase is not configured. Please check your .env.')
      return
    }

    try {
      const roomMeta = rooms.find((r) => r.id === selectedRoom)

      const { data: order, error: orderError } = await supabase
        .from('orders')
        .insert({
          room_code: selectedRoom,
          room_label: roomMeta ? roomMeta.ward : null,
          status: 'pending'
        })
        .select('*')
        .single()

      if (orderError || !order) {
        console.error('Error creating order', orderError)
        alert('Could not place order. Please try again.')
        return
      }

      const itemsPayload = orderItems.map((item) => ({
        order_id: order.id,
        medicine_id: item.medicineId,
        quantity: item.qty || 1,
        unit_price: item.unitPrice || 0,
        mrp: item.unitMrp || 0
      }))

      const { error: itemsError } = await supabase
        .from('order_items')
        .insert(itemsPayload)

      if (itemsError) {
        console.error('Error creating order items', itemsError)
        alert('Order created but items could not be saved.')
        return
      }

      // Clear cart after successful order
      try {
        window.localStorage.removeItem('medrover_cart')
      } catch (e) {
        console.error('Failed to clear cart after order', e)
      }

      navigate('/order-success')
    } catch (e) {
      console.error('Exception while placing order', e)
      alert('Unexpected error while placing order.')
    }
  }

  return (
    <>
      <Navbar
        variant="plain"
        cartCount={cartCount}
      />

      {/* ===== SELECT DELIVERY ROOM PAGE ===== */}
      <section className="select-room-section">
        <div className="container">

          {/* Breadcrumb */}
          <nav className="cart-breadcrumb">
            <Link to="/">Home</Link>
            <i className="bi bi-chevron-right"></i>
            <Link to="/cart">Cart</Link>
            <i className="bi bi-chevron-right"></i>
            <span>Select Delivery Room</span>
          </nav>

          <div className="row g-4">

            {/* LEFT: Room Selection */}
            <div className="col-lg-8">

              {/* Page Header */}
              <div className="room-page-header">
                <h4 className="room-page-title">
                  <i className="bi bi-geo-alt-fill me-2"></i>Select Delivery Room
                </h4>
                <p className="room-page-subtitle">
                  Choose where the MedRover should deliver your medicines
                </p>
              </div>

              {/* Room Cards */}
              <div className="room-select-grid">
                {rooms.map((room) => (
                  <label className="room-select-card" key={room.id}>
                    <input
                      type="radio"
                      name="deliveryRoom"
                      value={room.id}
                      checked={selectedRoom === room.id}
                      onChange={() => setSelectedRoom(room.id)}
                    />
                    <div className="room-select-content">
                      <div className="room-select-icon-wrap">
                        <i className="bi bi-door-open-fill"></i>
                      </div>
                      <div className="room-select-info">
                        <h5 className="room-select-name">{room.name}</h5>
                        <p className="room-select-ward">{room.ward}</p>
                        <div className="room-select-details">
                          <span><i className="bi bi-building me-1"></i>{room.type}</span>
                          <span><i className="bi bi-clock me-1"></i>{room.time}</span>
                        </div>
                      </div>
                      <span className="room-select-check">
                        <i className="bi bi-check-lg"></i>
                      </span>
                    </div>
                  </label>
                ))}
              </div>

              {/* Robot Info Banner */}
              <div className="robot-info-banner">
                <div className="robot-info-icon">
                  <i className="bi bi-robot"></i>
                </div>
                <div className="robot-info-text">
                  <h6>Autonomous MedRover Delivery</h6>
                  <p>
                    Our robot uses line-following navigation with junction detection to deliver
                    medicines safely to your selected room.
                  </p>
                </div>
              </div>

            </div>

            {/* RIGHT: Order Summary Sidebar */}
            <div className="col-lg-4">

              {/* Order Summary */}
              <div className="sidebar-card order-summary-card">
                <h6 className="order-summary-title">Order Summary</h6>

                {orderItems.length === 0 && (
                  <p className="small text-muted mb-2">
                    Your cart is empty. Go back to cart and add medicines.
                  </p>
                )}

                {orderItems.map((item, idx) => (
                  <div className="order-summary-item" key={idx}>
                    <span className="order-summary-name">{item.name}</span>
                    <span className="order-summary-price">
                      ₹{(item.unitPrice * item.qty).toFixed(2)}
                    </span>
                  </div>
                ))}

                <hr className="bill-divider" />

                <div className="bill-row">
                  <span className="bill-label">Total MRP</span>
                  <span className="bill-value">
                    ₹{cartTotals.totalMrp.toFixed(2)}
                  </span>
                </div>

                <div className="bill-row bill-total-row">
                  <span className="bill-label">Total Amount</span>
                  <span className="bill-value">
                    ₹{cartTotals.totalAmount.toFixed(2)}
                  </span>
                </div>

                {savings > 0 && (
                  <div className="bill-savings">
                    <i className="bi bi-tag-fill"></i>
                    You save <strong>₹{savings.toFixed(2)}</strong> on this order
                  </div>
                )}
              </div>

              {/* Delivery Info Card */}
              <div className="sidebar-card delivery-info-card">
                <h6 className="delivery-info-title">
                  <i className="bi bi-info-circle-fill me-2"></i>Delivery Information
                </h6>
                <ul className="delivery-info-list">
                  <li>
                    <i className="bi bi-check-circle-fill"></i>
                    <span>Robot navigates autonomously</span>
                  </li>
                  <li>
                    <i className="bi bi-check-circle-fill"></i>
                    <span>Contactless medicine delivery</span>
                  </li>
                  <li>
                    <i className="bi bi-check-circle-fill"></i>
                    <span>Real-time order tracking</span>
                  </li>
                  <li>
                    <i className="bi bi-check-circle-fill"></i>
                    <span>IR sensor based navigation</span>
                  </li>
                </ul>
              </div>

              {/* Place Order Button */}
              <button className="btn-place-order-final" onClick={handlePlaceOrder}>
                <i className="bi bi-send-fill me-2"></i>Place Order
              </button>

            </div>

          </div>
        </div>
      </section>

      <Footer variant="simple" />
    </>
  )
}

export default SelectRoomPage

