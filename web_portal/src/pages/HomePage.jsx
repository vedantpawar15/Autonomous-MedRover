import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'
import { supabase } from '../lib/supabaseClient'
import { cartTotalQty, readCartLines } from '../lib/cartStorage'

function HomePage() {
  const navigate = useNavigate()
  const [cartCount, setCartCount] = useState(0)
  const [searchTerm, setSearchTerm] = useState('')
  const [suggestions, setSuggestions] = useState([])
  const [showSuggestions, setShowSuggestions] = useState(false)

  // Simple one-time Supabase connectivity test
  useEffect(() => {
    if (!supabase) return

    const testSupabaseConnection = async () => {
      try {
        const { data, error } = await supabase
          .from('medicines')
          .select('*')
          .limit(1)

        if (error) {
          console.error('Supabase test error (but connection works):', error)
        } else {
          console.log('Supabase connection OK, sample data:', data)
        }
      } catch (err) {
        console.error('Supabase test exception:', err)
      }
    }

    testSupabaseConnection()
  }, [])

  // Navbar badge: stay in sync when cart updates from search/cart pages
  useEffect(() => {
    const refresh = () => setCartCount(cartTotalQty(readCartLines()))
    refresh()
    window.addEventListener('medrover_cart_changed', refresh)
    window.addEventListener('focus', refresh)
    return () => {
      window.removeEventListener('medrover_cart_changed', refresh)
      window.removeEventListener('focus', refresh)
    }
  }, [])

  const handleSearchSubmit = (e) => {
    e.preventDefault()
    const query = searchTerm
    if (query.trim()) {
      setShowSuggestions(false)
      navigate(`/search?query=${encodeURIComponent(query)}`)
    }
  }

  const handleSearchChange = (e) => {
    setSearchTerm(e.target.value)
    setShowSuggestions(true)
  }

  const handleSuggestionClick = (name) => {
    setSearchTerm(name)
    setShowSuggestions(false)
    navigate(`/search?query=${encodeURIComponent(name)}`)
  }

  // Fetch medicine name suggestions as user types
  useEffect(() => {
    const term = searchTerm.trim()
    if (!supabase || term.length < 2) {
      setSuggestions([])
      return
    }

    const timeoutId = setTimeout(async () => {
      try {
        const { data, error } = await supabase
          .from('medicines')
          .select('id, name')
          .ilike('name', `${term}%`)
          .order('name', { ascending: true })
          .limit(6)

        if (error) {
          console.error('Error fetching suggestions', error)
          setSuggestions([])
        } else {
          setSuggestions(data || [])
        }
      } catch (err) {
        console.error('Exception fetching suggestions', err)
        setSuggestions([])
      }
    }, 250) // small debounce

    return () => clearTimeout(timeoutId)
  }, [searchTerm])

  return (
    <>
      <Navbar variant="home" cartCount={cartCount} />

      {/* ===== HERO SECTION ===== */}
      <section className="hero-section">
        <div className="container text-center">
          <h1 className="hero-title">
            <span className="text-highlight">Get Medicines Delivered</span>
            <br />Autonomously to Any Room
          </h1>
          <p className="hero-subtitle">
            Fast, contactless &amp; robot-powered medicine delivery inside your hospital
          </p>

          {/* Order Card inside hero */}
          <div className="order-float-card mx-auto">

            {/* Section Label */}
            <div className="card-divider">
              <span>ORDER &amp; AVAIL MAX DISCOUNTS</span>
            </div>

            {/* Search Bar */}
            <form className="hero-search-wrapper" onSubmit={handleSearchSubmit}>
              <i className="bi bi-search hero-search-icon"></i>
              <input
                type="text"
                className="hero-search-input"
                placeholder="Search for Medicines..."
                autoComplete="off"
                value={searchTerm}
                onChange={handleSearchChange}
              />
              <button type="submit" className="btn-hero-search">Search</button>
            </form>

            {showSuggestions && suggestions.length > 0 && (
              <div className="hero-suggestions">
                <div className="hero-suggestions-header">
                  Showing results for <span>{searchTerm}</span>
                </div>
                {suggestions.map((med) => (
                  <button
                    key={med.id}
                    type="button"
                    className="hero-suggestion-item"
                    onClick={() => handleSuggestionClick(med.name)}
                  >
                    <span className="hero-suggestion-name">{med.name}</span>
                    <i className="bi bi-search hero-suggestion-icon"></i>
                  </button>
                ))}
              </div>
            )}

            {/* Or You Can Order Via */}
            <div className="or-divider">
              <span>OR YOU CAN ORDER VIA</span>
            </div>

            <div className="order-via-row">
              <a href="#" className="order-via-card">
                <i className="bi bi-clipboard2-pulse"></i>
                <span>Browse Medicine List</span>
              </a>
              <a href="#" className="order-via-card">
                <i className="bi bi-telephone-fill"></i>
                <span>Call Pharmacy Desk</span>
              </a>
            </div>

          </div>

          {/* Divider Badge */}
          <div className="divider-badge mt-4">
            <span>ONLY ON MEDROVER</span>
          </div>

          {/* Feature Cards Row */}
          <div className="row g-2 justify-content-center mt-3" id="features">
            <div className="col-md-3 col-sm-4">
              <div className="feature-card">
                <div className="feature-icon">
                  <i className="bi bi-robot"></i>
                </div>
                <div>
                  <h6>Autonomous Robot</h6>
                  <p>Smart navigation</p>
                </div>
              </div>
            </div>
            <div className="col-md-3 col-sm-4">
              <div className="feature-card featured">
                <div className="feature-icon">
                  <i className="bi bi-lightning-charge-fill"></i>
                </div>
                <div>
                  <h6>Express Delivery</h6>
                  <p>Fast &amp; contactless</p>
                </div>
              </div>
            </div>
            <div className="col-md-3 col-sm-4">
              <div className="feature-card">
                <div className="feature-icon">
                  <i className="bi bi-broadcast"></i>
                </div>
                <div>
                  <h6>Real-time Tracking</h6>
                  <p>Live status updates</p>
                </div>
              </div>
            </div>
          </div>

        </div>
      </section>

      {/* ===== HOW IT WORKS ===== */}
      <section className="how-section" id="how-it-works">
        <div className="container text-center">
          <h2 className="section-title">How It Works</h2>
          <p className="section-subtitle">Simple 4-step delivery process</p>

          <div className="row g-4 mt-3">
            <div className="col-lg-3 col-sm-6">
              <div className="step-card">
                <div className="step-number">1</div>
                <i className="bi bi-clipboard2-pulse step-icon"></i>
                <h6>Place Order</h6>
                <p>Staff selects medicine &amp; destination room from the web portal</p>
              </div>
            </div>
            <div className="col-lg-3 col-sm-6">
              <div className="step-card">
                <div className="step-number">2</div>
                <i className="bi bi-database-fill-gear step-icon"></i>
                <h6>Order Stored</h6>
                <p>Order is saved to Supabase cloud database instantly</p>
              </div>
            </div>
            <div className="col-lg-3 col-sm-6">
              <div className="step-card">
                <div className="step-number">3</div>
                <i className="bi bi-cpu step-icon"></i>
                <h6>Robot Picks Up</h6>
                <p>ESP32 robot detects new order &amp; begins autonomous navigation</p>
              </div>
            </div>
            <div className="col-lg-3 col-sm-6">
              <div className="step-card">
                <div className="step-number">4</div>
                <i className="bi bi-check-circle-fill step-icon"></i>
                <h6>Delivered!</h6>
                <p>Medicine delivered to room &amp; order status updated automatically</p>
              </div>
            </div>
          </div>
        </div>
      </section>

      {/* ===== FAQ SECTION ===== */}
      <section className="faq-section" id="faq">
        <div className="container">
          <h2 className="section-title text-center">Frequently Asked Questions</h2>

          <div className="faq-list mx-auto mt-4">

            <details className="faq-item" open>
              <summary>How does the MedRover robot navigate?</summary>
              <p>
                The robot uses IR line-following sensors to follow paths on the hospital floor.
                It detects junctions to make turns and reach the correct destination room (A, B, or C).
              </p>
            </details>

            <details className="faq-item">
              <summary>Is the delivery truly autonomous?</summary>
              <p>
                Yes! Once an order is placed, the ESP32 microcontroller on the robot polls the database,
                picks up the order, navigates autonomously, and updates the status — all without human intervention.
              </p>
            </details>

            <details className="faq-item">
              <summary>How fast is the delivery?</summary>
              <p>
                Delivery time depends on the distance to the destination room. Typically, the robot can
                deliver medicine within 2–5 minutes inside the hospital premises.
              </p>
            </details>

            <details className="faq-item">
              <summary>What technology stack is used?</summary>
              <p>
                The frontend uses React with Vite &amp; Bootstrap. The backend uses Supabase (PostgreSQL + REST APIs).
                The robot runs on an ESP32 with L298N motor driver and IR sensors.
              </p>
            </details>

            <details className="faq-item">
              <summary>Can I track my order?</summary>
              <p>
                Yes, the web portal shows real-time order status — Pending, In Transit, or Delivered —
                updated automatically by the robot.
              </p>
            </details>

          </div>
        </div>
      </section>

      <Footer variant="full" />
    </>
  )
}

export default HomePage

