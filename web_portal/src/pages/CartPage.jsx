import { useState, useMemo, useEffect } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'

function CartPage() {
  const navigate = useNavigate()
  const [items, setItems] = useState([])

  // Load cart from localStorage (populated by SearchPage)
  useEffect(() => {
    try {
      const raw = window.localStorage.getItem('medrover_cart')
      if (!raw) return
      const parsed = JSON.parse(raw)
      if (Array.isArray(parsed)) {
        setItems(parsed)
      }
    } catch (e) {
      console.error('Failed to load cart from localStorage', e)
    }
  }, [])

  const handleNavSearch = (query) => {
    const trimmed = query.trim()
    if (!trimmed) return
    navigate(`/search?query=${encodeURIComponent(trimmed)}`)
  }

  const handleDeleteItem = (id) => {
    setItems((prev) => {
      const next = prev.filter((item) => item.id !== id)
      try {
        window.localStorage.setItem('medrover_cart', JSON.stringify(next))
      } catch (e) {
        console.error('Failed to persist cart from CartPage (delete)', e)
      }
      return next
    })
  }

  const handleQtyChange = (id, qty) => {
    setItems((prev) => {
      const next = prev.map((item) =>
        item.id === id ? { ...item, selectedQty: qty } : item
      )
      try {
        window.localStorage.setItem('medrover_cart', JSON.stringify(next))
      } catch (e) {
        console.error('Failed to persist cart from CartPage (qty)', e)
      }
      return next
    })
  }

  const { totalMrp, totalCurrent, totalSavings, totalQuantity } = useMemo(() => {
    return items.reduce(
      (acc, item) => {
        acc.totalMrp += item.originalPrice * item.selectedQty
        acc.totalCurrent += item.currentPrice * item.selectedQty
        acc.totalQuantity += item.selectedQty
        return acc
      },
      { totalMrp: 0, totalCurrent: 0, totalSavings: 0, totalQuantity: 0 }
    )
  }, [items])

  const cartTotal = totalCurrent
  const savings = totalMrp - totalCurrent

  return (
    <>
      <Navbar
        variant="inner"
        onSearch={handleNavSearch}
        cartCount={totalQuantity}
      />

      {/* ===== CART PAGE ===== */}
      <section className="cart-page-section">
        <div className="container">

          {/* Breadcrumb */}
          <nav className="cart-breadcrumb">
            <Link to="/">Home</Link>
            <i className="bi bi-chevron-right"></i>
            <span>Cart</span>
          </nav>

          <div className="row g-4">

            {/* LEFT: Cart Items */}
            <div className="col-lg-8">

              {/* Cart Header */}
              <div className="cart-header-card">
                <h4 className="cart-header-title">
                  {items.length} Product{items.length === 1 ? '' : 's'} in your Cart
                </h4>
              </div>

              {/* Cart Items */}
              {items.length === 0 && (
                <p className="mt-3">Your cart is empty. Add medicines from the search page.</p>
              )}
              {items.map((item) => (
                <div className="cart-item-card" key={item.id}>
                  <div className="cart-item-img">
                    <i className="bi bi-capsule"></i>
                  </div>
                  <div className="cart-item-body">
                      <div className="cart-item-top">
                      <div className="cart-item-info">
                        <h6 className="cart-item-name">{item.name}</h6>
                        <p className="cart-item-qty-info">{item.qtyInfo}</p>
                      </div>
                      <button
                        className="cart-item-delete"
                        title="Remove item"
                        type="button"
                        onClick={() => handleDeleteItem(item.id)}
                      >
                        <i className="bi bi-trash3"></i>
                      </button>
                    </div>
                      <div className="cart-item-bottom">
                      <div className="cart-item-price-row">
                      <span className="cart-price-original">
                          ₹{item.originalPrice.toFixed(2)}
                        </span>
                        <span className="cart-price-current">
                          ₹{item.currentPrice.toFixed(2)}
                        </span>
                      </div>
                      <select
                        className="qty-select"
                        value={item.selectedQty}
                        onChange={(e) =>
                          handleQtyChange(item.id, Number(e.target.value))
                        }
                      >
                        {[1, 2, 3, 4, 5].map((n) => (
                          <option key={n} value={n}>Qty {n}</option>
                        ))}
                      </select>
                    </div>
                  </div>
                </div>
              ))}

            </div>

            {/* RIGHT: Sidebar */}
            <div className="col-lg-4">

              {/* Cart Total */}
              <div className="sidebar-card cart-total-card">
                <div className="cart-total-row">
                  <span className="cart-total-label">Cart total:</span>
                  <span className="cart-total-amount">
                    ₹{cartTotal.toFixed(2)}
                  </span>
                </div>
              </div>

              {/* Select Delivery Room Button */}
              <Link to="/select-room" className="btn-delivery-room">
                <span>Select Delivery Room</span>
                <i className="bi bi-arrow-right-circle-fill"></i>
              </Link>

              {/* Bill Summary */}
              <div className="sidebar-card bill-summary-card">
                <h6 className="bill-summary-title">Bill Summary</h6>
                <div className="bill-row">
                  <span className="bill-label">Total MRP</span>
                  <span className="bill-value">₹{totalMrp.toFixed(2)}</span>
                </div>
                <div className="bill-row">
                  <span className="bill-label">Discount</span>
                  <span className="bill-value bill-discount">
                    - ₹{savings.toFixed(2)}
                  </span>
                </div>
                <div className="bill-row">
                  <span className="bill-label">Delivery charges</span>
                  <span className="bill-value bill-free">FREE</span>
                </div>
                <hr className="bill-divider" />
                <div className="bill-row bill-total-row">
                  <span className="bill-label">Total Amount</span>
                  <span className="bill-value">₹{cartTotal.toFixed(2)}</span>
                </div>
                <div className="bill-savings">
                  <i className="bi bi-tag-fill"></i>
                  You save <strong>₹{savings.toFixed(2)}</strong> on this order
                </div>
              </div>

            </div>

          </div>
        </div>
      </section>

      <Footer variant="simple" />
    </>
  )
}

export default CartPage

