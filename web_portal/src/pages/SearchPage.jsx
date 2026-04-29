import { useState, useMemo, useEffect } from 'react'
import { useSearchParams, useNavigate, Link } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'
import {
  mergeSearchItemsIntoStoredCart,
  persistCartLines,
  cartTotalQty
} from '../lib/cartStorage'

const formatMedicine = (row) => ({
  id: row.id,
  name: row.name,
  brand: row.brand,
  qty: row.pack_info,
  image: row.image_url || '',
  price: Number(row.mrp) || 0,
  rx: !!row.requires_rx,
  inCart: false,
  cartQty: 1
})

function SearchPage() {
  const [searchParams] = useSearchParams()
  const navigate = useNavigate()
  const query = searchParams.get('query') || 'Dolo'

  const [items, setItems] = useState([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')

  useEffect(() => {
    const fetchMedicines = async () => {
      if (!supabase) return
      setLoading(true)
      setError('')
      try {
        let q = supabase.from('medicines').select('*')

        if (query) {
          q = q.ilike('name', `%${query}%`)
        }

        const { data, error: err } = await q.limit(50)
        if (err) {
          console.error('Error loading medicines', err)
          setError('Could not load medicines from server.')
        } else {
          // Sync with any existing cart in localStorage
          let cartById = new Map()
          try {
            const raw = window.localStorage.getItem('medrover_cart')
            if (raw) {
              const parsed = JSON.parse(raw)
              if (Array.isArray(parsed)) {
                cartById = new Map(
                  parsed.map((item) => [item.id, item.selectedQty || 1])
                )
              }
            }
          } catch (e) {
            console.error('Failed to read cart from localStorage on SearchPage', e)
          }

          const mapped = data.map((row) => {
            const base = formatMedicine(row)
            const qty = cartById.get(base.id)
            if (qty) {
              return { ...base, inCart: true, cartQty: qty }
            }
            return base
          })

          setItems(mapped)
        }
      } catch (e) {
        console.error('Exception loading medicines', e)
        setError('Unexpected error loading medicines.')
      } finally {
        setLoading(false)
      }
    }

    fetchMedicines()
  }, [query])

  // Merge visible cart rows into full cart so switching searches does not drop items
  useEffect(() => {
    try {
      const merged = mergeSearchItemsIntoStoredCart(items)
      persistCartLines(merged)
    } catch (e) {
      console.error('Failed to persist merged cart', e)
    }
  }, [items])

  const mergedCartLines = useMemo(
    () => mergeSearchItemsIntoStoredCart(items),
    [items]
  )

  const filteredItems = useMemo(
    () =>
      items.filter((med) =>
        med.name.toLowerCase().includes(query.toLowerCase())
      ),
    [items, query]
  )

  const cartCount = cartTotalQty(mergedCartLines)

  const handleNavSearch = (q) => {
    const trimmed = q.trim()
    if (!trimmed) return
    navigate(`/search?query=${encodeURIComponent(trimmed)}`)
  }

  const handleAddToCart = (id) => {
    setItems((prev) =>
      prev.map((med) =>
        med.id === id ? { ...med, inCart: true, cartQty: 1 } : med
      )
    )
  }

  const handleQtyChange = (id, qty) => {
    setItems((prev) =>
      prev.map((med) =>
        med.id === id ? { ...med, cartQty: qty } : med
      )
    )
  }

  return (
    <>
      <Navbar
        variant="plain"
        cartCount={cartCount}
      />

      {/* ===== SEARCH RESULTS ===== */}
      <section className="search-results-section">
        <div className="container">

          <h4 className="results-heading">
            Showing all results for <strong>{query}</strong>
          </h4>

          <div className="row g-4">

            {/* LEFT: Results */}
              <div className="col-lg-8">
              {loading && <p>Loading medicines from MedRover cloud...</p>}
              {error && <p className="text-danger small">{error}</p>}

              {!loading && !error && filteredItems.length === 0 && query && (
                <div className="medicine-card">
                  <div className="medicine-img">
                    <i className="bi bi-capsule"></i>
                  </div>
                  <div className="medicine-body">
                    <div className="medicine-top-row">
                      <div className="medicine-info">
                        <h5 className="medicine-name">{query}</h5>
                        <p className="medicine-brand text-muted">
                          This medicine is currently not available in MedRover stock.
                        </p>
                      </div>
                      <span className="rx-badge" title="Out of stock">
                        <b>Out</b>
                      </span>
                    </div>
                    <div className="medicine-bottom-row">
                      <div className="medicine-price-row">
                        <span className="price-current text-muted">Out of stock</span>
                      </div>
                    </div>
                  </div>
                </div>
              )}

              {!loading && !error && filteredItems.map((med) => (
                <div className="medicine-card" key={med.id}>
                  <div className="medicine-img">
                    {med.image ? (
                      <img
                        src={med.image}
                        alt={med.name}
                        loading="lazy"
                        onError={(e) => {
                          e.currentTarget.style.display = 'none'
                          e.currentTarget.parentElement
                            ?.querySelector('.medicine-img-fallback')
                            ?.classList.add('show')
                        }}
                      />
                    ) : null}
                    <i
                      className={`bi bi-capsule medicine-img-fallback${
                        med.image ? '' : ' show'
                      }`}
                    ></i>
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
                        <span className="price-current">₹{med.price.toFixed(2)}</span>
                      </div>
                      <div className="medicine-action">
                        {med.inCart ? (
                          <select
                            className="qty-select"
                            value={med.cartQty || 1}
                            onChange={(e) =>
                              handleQtyChange(med.id, Number(e.target.value))
                            }
                          >
                            {[1, 2, 3, 4, 5].map((n) => (
                              <option key={n} value={n}>Qty {n}</option>
                            ))}
                          </select>
                        ) : (
                          <button
                            className="btn-add-cart"
                            type="button"
                            onClick={() => handleAddToCart(med.id)}
                          >
                            Add To Cart
                          </button>
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
              <p className="cart-items-count">
                  {cartCount} Item{cartCount === 1 ? '' : 's'} in Cart
                </p>
                <p className="small text-muted mb-2">
                  Search again to add more medicines, then open your cart and choose a delivery room.
                </p>
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

