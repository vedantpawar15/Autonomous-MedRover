import { useState, useMemo, useEffect } from 'react'
import { Link, useNavigate, useLocation } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { persistCartLines, readCartLines } from '../lib/cartStorage'

function CartPage() {
  const navigate = useNavigate()
  const location = useLocation()
  const [items, setItems] = useState([])

  const loadCart = () => {
    try {
      setItems(readCartLines())
    } catch (e) {
      console.error('Failed to load cart from localStorage', e)
    }
  }

  // Reload whenever user opens /cart (e.g. after adding items from search)
  useEffect(() => {
    loadCart()
  }, [location.pathname])

  useEffect(() => {
    const onCartChanged = () => loadCart()
    window.addEventListener('medrover_cart_changed', onCartChanged)
    return () => window.removeEventListener('medrover_cart_changed', onCartChanged)
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
        persistCartLines(next)
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
        persistCartLines(next)
      } catch (e) {
        console.error('Failed to persist cart from CartPage (qty)', e)
      }
      return next
    })
  }

  const { totalMrp, totalQuantity } = useMemo(() => {
    return items.reduce(
      (acc, item) => {
        const qty = item.selectedQty || 1
        acc.totalMrp += (item.price || 0) * qty
        acc.totalQuantity += qty
        return acc
      },
      { totalMrp: 0, totalQuantity: 0 }
    )
  }, [items])

  const cartTotal = totalMrp

  return (
    <>
      <Navbar
        variant="plain"
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
                    {item.image ? (
                      <img
                        src={item.image}
                        alt={item.name}
                        loading="lazy"
                        onError={(e) => {
                          e.currentTarget.style.display = 'none'
                          e.currentTarget.parentElement
                            ?.querySelector('.cart-item-img-fallback')
                            ?.classList.add('show')
                        }}
                      />
                    ) : null}
                    <i
                      className={`bi bi-capsule cart-item-img-fallback${
                        item.image ? '' : ' show'
                      }`}
                    ></i>
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
                        <span className="cart-price-current">
                          ₹{(item.price || 0).toFixed(2)}
                        </span>
                      </div>
                      <select
                        className="qty-select"
                        value={item.selectedQty || 1}
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

              {/* Select Delivery Room — only after cart has items */}
              {items.length > 0 ? (
                <Link to="/select-room" className="btn-delivery-room">
                  <span>Select Delivery Room</span>
                  <i className="bi bi-arrow-right-circle-fill"></i>
                </Link>
              ) : (
                <button type="button" className="btn-delivery-room" disabled aria-disabled="true">
                  <span>Add medicines first</span>
                  <i className="bi bi-arrow-right-circle-fill"></i>
                </button>
              )}

              {/* Bill Summary */}
              <div className="sidebar-card bill-summary-card">
                <h6 className="bill-summary-title">Bill Summary</h6>
                <div className="bill-row">
                  <span className="bill-label">Total MRP</span>
                  <span className="bill-value">₹{totalMrp.toFixed(2)}</span>
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

