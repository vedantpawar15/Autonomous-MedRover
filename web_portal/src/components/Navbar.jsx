import { Link, useLocation } from 'react-router-dom'

/**
 * Navbar — Shared across all pages
 * Props:
 *  - variant: "home" | "inner"  (home has delivery info, inner has search bar)
 *  - searchValue: string  (pre-fill search input on search page)
 *  - onSearch: (query) => void
 *  - cartCount: number
 */
function Navbar({ variant = 'inner', searchValue = '', onSearch, cartCount = 4 }) {
  const location = useLocation()

  const handleSearchSubmit = (e) => {
    e.preventDefault()
    const query = e.target.elements.query.value
    if (onSearch) onSearch(query)
  }

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
          <div className="nav-separator"></div>

          {/* Home variant: delivery info */}
          {variant === 'home' && (
            <div className="nav-delivery">
              <span className="delivery-label">
                <i className="bi bi-lightning-charge-fill text-warning"></i> Robot delivery to
              </span>
              <span className="delivery-select">
                Select Room <i className="bi bi-chevron-down"></i>
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
                <i className="bi bi-search"></i>
              </button>
            </form>
          )}

          {/* Plain variant: no search bar, no delivery info (Cart / Orders / Room pages) */}
          {variant === 'plain' && <div className="nav-plain-spacer" />}

          {/* Right Actions */}
          <div className="nav-top-right ms-auto d-flex align-items-center">
            <Link to="#" className="nav-top-action">
              <i className="bi bi-person"></i>
              <span>Hello, Log in</span>
            </Link>
            <Link to="/orders" className="nav-top-action">
              <i className="bi bi-clipboard2-pulse"></i>
              <span>Orders</span>
            </Link>
            <Link to="/cart" className="nav-top-action nav-cart-action">
              <i className="bi bi-cart3"></i>
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
            <i className="bi bi-list text-dark" style={{ fontSize: '1.5rem' }}></i>
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
            </ul>
          </div>
        </div>
      </div>
    </>
  )
}

export default Navbar

