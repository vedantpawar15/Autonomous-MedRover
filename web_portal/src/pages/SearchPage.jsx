import { useSearchParams, useNavigate, Link } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'

const medicines = [
  {
    id: 1,
    name: 'Dolo 650mg Strip Of 15 Tablets',
    brand: 'MICRO LABS',
    qty: '15 Tablet(s) in Strip',
    price: 24.10,
    original: 32.13,
    discount: '25% OFF',
    rx: true,
    inCart: true,
    cartQty: 2
  },
  {
    id: 2,
    name: 'Leemol 650mg Strip Of 15 Tablets',
    brand: 'LEEFORD HEALTHCARE LTD',
    qty: '15 Tablet(s) in Strip',
    price: 16.70,
    original: 31.50,
    discount: '47% OFF',
    rx: true,
    inCart: false
  },
  {
    id: 3,
    name: 'Dolo 500mg Strip Of 15 Tablets',
    brand: 'MICRO LABS',
    qty: '15 Tablet(s) in Strip',
    price: 10.63,
    original: 14.18,
    discount: '25% OFF',
    rx: false,
    inCart: false
  },
  {
    id: 4,
    name: 'Dolonex DT 20mg Strip Of 15 Tablets',
    brand: 'CIPLA LIMITED',
    qty: '15 Tablet(s) in Strip',
    price: 201.59,
    original: 268.79,
    discount: '25% OFF',
    rx: true,
    inCart: false
  },
  {
    id: 5,
    name: 'Dolo 250mg Bottle Of 60ml Suspension',
    brand: 'MICRO LABS',
    qty: '60ml Suspension in Bottle',
    price: 32.10,
    original: 42.80,
    discount: '25% OFF',
    rx: false,
    inCart: false
  },
  {
    id: 6,
    name: 'Dolopar 650mg Strip Of 15 Tablets',
    brand: 'MICRO LABS',
    qty: '15 Tablet(s) in Strip',
    price: 24.08,
    original: 32.10,
    discount: '25% OFF',
    rx: false,
    inCart: false
  }
]

function SearchPage() {
  const [searchParams] = useSearchParams()
  const navigate = useNavigate()
  const query = searchParams.get('query') || 'Dolo'

  const handleNavSearch = (q) => {
    navigate(`/search?query=${encodeURIComponent(q)}`)
  }

  return (
    <>
      <Navbar variant="inner" searchValue={query} onSearch={handleNavSearch} />

      {/* ===== SEARCH RESULTS ===== */}
      <section className="search-results-section">
        <div className="container">

          <h4 className="results-heading">
            Showing all results for <strong>{query}</strong>
          </h4>

          <div className="row g-4">

            {/* LEFT: Results */}
            <div className="col-lg-8">
              {medicines.map((med) => (
                <div className="medicine-card" key={med.id}>
                  <div className="medicine-img">
                    <i className="bi bi-capsule"></i>
                  </div>
                  <div className="medicine-body">
                    <div className="medicine-top-row">
                      <div className="medicine-info">
                        <h5 className="medicine-name">{med.name}</h5>
                        <p className="medicine-brand">By {med.brand}</p>
                        <p className="medicine-qty">{med.qty}</p>
                      </div>
                      {med.rx && (
                        <span className="rx-badge" title="Prescription Required">
                          <b>℞</b>
                        </span>
                      )}
                    </div>
                    <div className="medicine-bottom-row">
                      <div className="medicine-price-row">
                        <span className="price-current">₹{med.price.toFixed(2)}*</span>
                        <span className="price-original">₹{med.original.toFixed(2)}</span>
                        <span className="price-discount">{med.discount}</span>
                      </div>
                      <div className="medicine-action">
                        {med.inCart ? (
                          <select className="qty-select" defaultValue={med.cartQty}>
                            {[1, 2, 3, 4, 5].map((n) => (
                              <option key={n} value={n}>Qty {n}</option>
                            ))}
                          </select>
                        ) : (
                          <button className="btn-add-cart">Add To Cart</button>
                        )}
                      </div>
                    </div>
                  </div>
                </div>
              ))}
            </div>

            {/* RIGHT: Sidebar */}
            <div className="col-lg-4">

              {/* Cart Summary */}
              <div className="sidebar-card cart-summary-card">
                <p className="cart-items-count">4 Items in Cart</p>
                <Link to="/cart" className="btn-view-cart">
                  View Cart <i className="bi bi-chevron-right"></i>
                </Link>
              </div>

              {/* Prescription Info */}
              <div className="sidebar-card prescription-card">
                <div className="prescription-header">
                  <span className="rx-icon-circle"><b>℞</b></span>
                  <h6 className="prescription-title">What is a valid prescription?</h6>
                </div>
                <hr className="prescription-divider" />
                <p className="sidebar-desc">A valid prescription contains:</p>
                <ul className="prescription-list">
                  <li>
                    <span className="presc-icon presc-icon-doctor">
                      <i className="bi bi-person-badge-fill"></i>
                    </span>
                    <span>Doctor Details</span>
                  </li>
                  <li>
                    <span className="presc-icon presc-icon-date">
                      <i className="bi bi-calendar3"></i>
                    </span>
                    <span>Date of Prescription</span>
                  </li>
                  <li>
                    <span className="presc-icon presc-icon-patient">
                      <i className="bi bi-person-vcard-fill"></i>
                    </span>
                    <span>Patient Details</span>
                  </li>
                  <li>
                    <span className="presc-icon presc-icon-dosage">
                      <i className="bi bi-prescription2"></i>
                    </span>
                    <span>Dosage Details</span>
                  </li>
                </ul>
              </div>

            </div>

          </div>
        </div>
      </section>

      <Footer variant="simple" />
    </>
  )
}

export default SearchPage

