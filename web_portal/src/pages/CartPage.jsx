import { Link, useNavigate } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'

const cartItems = [
  {
    id: 1,
    name: 'Dolo 650mg Strip Of 15 Tablets',
    qtyInfo: '15 Tablet(s) in Strip',
    originalPrice: 64.26,
    currentPrice: 57.84,
    selectedQty: 2
  },
  {
    id: 2,
    name: 'Dolonex DT 20mg Strip Of 15 Tablets',
    qtyInfo: '15 Tablet(s) in Strip',
    originalPrice: 537.58,
    currentPrice: 483.82,
    selectedQty: 2
  },
  {
    id: 3,
    name: 'Dolo 250mg Bottle Of 60ml Suspension',
    qtyInfo: '60ml Suspension in Bottle',
    originalPrice: 42.80,
    currentPrice: 32.10,
    selectedQty: 1
  },
  {
    id: 4,
    name: 'Leemol 650mg Strip Of 15 Tablets',
    qtyInfo: '15 Tablet(s) in Strip',
    originalPrice: 31.50,
    currentPrice: 16.70,
    selectedQty: 1
  }
]

function CartPage() {
  const navigate = useNavigate()

  const handleNavSearch = (query) => {
    navigate(`/search?query=${encodeURIComponent(query)}`)
  }

  return (
    <>
      <Navbar variant="inner" onSearch={handleNavSearch} />

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
                <h4 className="cart-header-title">{cartItems.length} Items in your Cart</h4>
              </div>

              {/* Cart Items */}
              {cartItems.map((item) => (
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
                      <button className="cart-item-delete" title="Remove item">
                        <i className="bi bi-trash3"></i>
                      </button>
                    </div>
                    <div className="cart-item-bottom">
                      <div className="cart-item-price-row">
                        <span className="cart-price-original">₹{item.originalPrice.toFixed(2)}</span>
                        <span className="cart-price-current">₹{item.currentPrice.toFixed(2)}</span>
                      </div>
                      <select className="qty-select" defaultValue={item.selectedQty}>
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
                  <span className="cart-total-amount">₹590.46</span>
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
                  <span className="bill-value">₹676.14</span>
                </div>
                <div className="bill-row">
                  <span className="bill-label">Discount</span>
                  <span className="bill-value bill-discount">- ₹85.68</span>
                </div>
                <div className="bill-row">
                  <span className="bill-label">Delivery charges</span>
                  <span className="bill-value bill-free">FREE</span>
                </div>
                <hr className="bill-divider" />
                <div className="bill-row bill-total-row">
                  <span className="bill-label">Total Amount</span>
                  <span className="bill-value">₹590.46</span>
                </div>
                <div className="bill-savings">
                  <i className="bi bi-tag-fill"></i>
                  You save <strong>₹85.68</strong> on this order
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

