import { Link, useLocation, useNavigate } from 'react-router-dom'
import { useAuth } from '../contexts/AuthContext'

/**
 * Navbar — Shared across all pages
 * Props:
 *  - variant: "home" | "inner" | "plain"
 *  - searchValue: string
 *  - onSearch: (query) => void
 *  - cartCount: number
 */
function Navbar({ variant = 'inner', searchValue = '', onSearch, cartCount = 0 }) {
  const location           = useLocation()
  const navigate           = useNavigate()
  const { user, signOut }  = useAuth()

  const handleSearchSubmit = (e) => {
    e.preventDefault()
    const query = e.target.elements.query.value
    if (onSearch) onSearch(query)
  }

  const handleLogout = async () => {
    await signOut()
    navigate('/')
  }

  // Show first part of email as greeting (e.g. "staff@hospital.com" → "staff")
  const displayName = user?.email?.split('@')[0] || 'Account'

  return (
    <>
      {/* ===== TOP BAR ===== */}
      <div className={`nav-top fixed-top nav-top--${variant}`}>
        <div className="container d-flex align-items-center">

          {/* Brand */}
          <Link className="nav-top-brand" to="/">
            <img src="/assets/logo/white.png" height="32" alt="Logo" />
            <div className="brand-info">
              <span className="brand-sub">Hospital Delivery</span>
              <span className="brand-name">MedRover</span>
            </div>
          </Link>

          {/* Separator */}
          <div className="nav-separator" />

          {/* Home variant: delivery info */}
          {variant === 'home' && (
            <div className="nav-delivery">
              <span className="delivery-label">
                <i className="bi bi-lightning-charge-fill text-warning" /> Robot delivery to
              </span>
              <span className="delivery-select">
                Select Room <i className="bi bi-chevron-down" />
              </span>
            </div>
          )}

          {/* Inner variant: search bar (Search page only) */}
          {variant === 'inner' && (
            <form className="nav-search-bar" onSubmit={handleSearchSubmit}>
              <input
                type="text"
                name="query"
                className="nav-search-input"
                placeholder="Search for Medicines..."
                defaultValue={searchValue}
                autoComplete="off"
              />
              <button type="submit" className="nav-search-btn">
                <i className="bi bi-search" />
              </button>
            </form>
          )}

          {/* Plain variant: spacer */}
          {variant === 'plain' && <div className="nav-plain-spacer" />}

          {/* ── Right actions — auth-aware ── */}
          <div className="nav-top-right ms-auto d-flex align-items-center">

            {user ? (
              /* Logged-in state */
              <>
                <div className="nav-user-pill">
                  <i className="bi bi-person-circle nav-user-icon" />
                  <span className="nav-user-name">{displayName}</span>
                </div>
                <button
                  className="nav-top-action nav-logout-btn"
                  onClick={handleLogout}
                  title="Sign out"
                >
                  <i className="bi bi-box-arrow-right" />
                  <span>Logout</span>
                </button>
              </>
            ) : (
              /* Logged-out state */
              <Link to="/login" className="nav-top-action nav-login-btn">
                <i className="bi bi-person" />
                <span>Log in</span>
              </Link>
            )}

            <Link to="/orders" className="nav-top-action">
              <i className="bi bi-clipboard2-pulse" />
              <span>Orders</span>
            </Link>

            <Link to="/cart" className="nav-top-action nav-cart-action">
              <i className="bi bi-cart3" />
              <span>Cart</span>
              {cartCount > 0 && <span className="cart-badge">{cartCount}</span>}
            </Link>
          </div>

          {/* Mobile Toggle */}
          <button
            className="navbar-toggler d-lg-none ms-2"
            type="button"
            data-bs-toggle="collapse"
            data-bs-target="#subNavbar"
          >
            <i className="bi bi-list text-dark" style={{ fontSize: '1.5rem' }} />
          </button>

        </div>
      </div>

      {/* ===== SUB BAR ===== */}
      <div className="nav-sub">
        <div className="container">
          <div className="collapse navbar-collapse d-lg-flex justify-content-center" id="subNavbar">
            <ul className="nav-sub-links">
              <li>
                <Link to="/" className={location.pathname === '/' ? 'active' : ''}>
                  Home
                </Link>
              </li>
              <li>
                {location.pathname === '/' ? (
                  <a href="#how-it-works">How It Works</a>
                ) : (
                  <Link to="/#how-it-works">How It Works</Link>
                )}
              </li>
              <li>
                {location.pathname === '/' ? (
                  <a href="#features">Features</a>
                ) : (
                  <Link to="/#features">Features</Link>
                )}
              </li>
              <li>
                {location.pathname === '/' ? (
                  <a href="#faq">FAQ</a>
                ) : (
                  <Link to="/#faq">FAQ</Link>
                )}
              </li>
              <li>
                <Link to="#">Contact</Link>
              </li>
              <li>
                <Link
                  to="/admin"
                  className={location.pathname === '/admin' ? 'active' : ''}
                  style={{ color: 'var(--primary)', fontWeight: 700 }}
                >
                  <i className="bi bi-shield-lock-fill me-1" style={{ fontSize: '0.8rem' }} />Admin
                </Link>
              </li>
            </ul>
          </div>
        </div>
      </div>
    </>
  )
}

export default Navbar
