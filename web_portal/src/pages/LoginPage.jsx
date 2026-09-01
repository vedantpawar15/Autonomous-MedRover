import { useState, useRef, useEffect } from 'react'
import { Link, useNavigate, useLocation } from 'react-router-dom'
import { useAuth } from '../contexts/AuthContext'

/**
 * LoginPage — staff-only login.
 * No public sign-up; accounts are created by admin in Supabase dashboard.
 * After login, redirects back to the page that required authentication.
 */
function LoginPage() {
  const { user, signIn } = useAuth()
  const navigate         = useNavigate()
  const location         = useLocation()
  const emailRef         = useRef(null)

  const [email,     setEmail]     = useState('')
  const [password,  setPassword]  = useState('')
  const [showPass,  setShowPass]  = useState(false)
  const [error,     setError]     = useState('')
  const [loading,   setLoading]   = useState(false)

  // Where to go after login — the page that originally required auth, or home
  const from = location.state?.from?.pathname || '/'

  // If already logged in, redirect immediately
  useEffect(() => {
    if (user) navigate(from, { replace: true })
  }, [user, from, navigate])

  useEffect(() => {
    emailRef.current?.focus()
  }, [])

  const handleSubmit = async (e) => {
    e.preventDefault()
    setError('')
    setLoading(true)

    const { error: err } = await signIn({ email: email.trim(), password })

    setLoading(false)

    if (err) {
      // Make error messages friendlier than Supabase defaults
      if (err.message.toLowerCase().includes('invalid login credentials') ||
          err.message.toLowerCase().includes('invalid credentials')) {
        setError('Incorrect email or password. Please try again.')
      } else if (err.message.toLowerCase().includes('email not confirmed')) {
        setError('Please confirm your email before logging in. Check your inbox.')
      } else {
        setError(err.message || 'Login failed. Please try again.')
      }
      return
    }

    // Success — navigate back
    navigate(from, { replace: true })
  }

  return (
    <div className="login-page">

      {/* ── Left branding panel ─────────────────────────────────── */}
      <div className="login-left">

        {/* Logo */}
        <div className="login-brand">
          <Link to="/" className="login-brand-link">
            <img src="/assets/logo/white.png" height="30" alt="MedRover" />
            <div>
              <div className="login-brand-name">MedRover</div>
              <div className="login-brand-tag">Hospital Delivery System</div>
            </div>
          </Link>
        </div>

        {/* Hero */}
        <div className="login-hero">
          <div className="login-eyebrow">Autonomous Medicine Delivery</div>
          <h1 className="login-headline">
            Care delivered<br />
            <span className="login-headline-accent">on time, every time.</span>
          </h1>
          <p className="login-subtext">
            The MedRover portal lets hospital staff order medicines directly
            to patient rooms — tracked and fulfilled by an autonomous robot.
          </p>

          <div className="login-features">
            {[
              { icon: 'bi-lightning-charge-fill', label: 'Instant room-to-room delivery'   },
              { icon: 'bi-shield-check',           label: 'Secure, staff-only access'        },
              { icon: 'bi-graph-up-arrow',         label: 'Real-time order tracking'         },
            ].map(f => (
              <div className="login-feature-item" key={f.label}>
                <span className="login-feature-icon">
                  <i className={`bi ${f.icon}`} />
                </span>
                <span>{f.label}</span>
              </div>
            ))}
          </div>
        </div>

        {/* Bottom */}
        <div className="login-left-footer">
          <span className="login-secured-badge">
            <i className="bi bi-lock-fill" />
            Secured by Supabase Auth
          </span>
        </div>
      </div>

      {/* ── Right form panel ─────────────────────────────────────── */}
      <div className="login-right">
        <div className="login-form-wrap">

          <div className="login-form-eyebrow">Staff Portal</div>
          <h2 className="login-form-title">Sign in to your account</h2>
          <p className="login-form-sub">
            Use the credentials provided by your hospital administrator.
          </p>

          <form onSubmit={handleSubmit} noValidate>

            {/* Email */}
            <div className="login-field">
              <label className="login-field-label" htmlFor="loginEmail">
                Email address
              </label>
              <div className={`login-input-wrap${error ? ' input-error' : ''}`}>
                <i className="bi bi-envelope login-input-icon" />
                <input
                  id="loginEmail"
                  ref={emailRef}
                  type="email"
                  className="login-input"
                  placeholder="staff@hospital.com"
                  autoComplete="email"
                  value={email}
                  onChange={e => { setEmail(e.target.value); setError('') }}
                  required
                />
              </div>
            </div>

            {/* Password */}
            <div className="login-field">
              <label className="login-field-label" htmlFor="loginPassword">
                Password
              </label>
              <div className={`login-input-wrap${error ? ' input-error' : ''}`}>
                <i className="bi bi-key login-input-icon" />
                <input
                  id="loginPassword"
                  type={showPass ? 'text' : 'password'}
                  className="login-input"
                  placeholder="Your password"
                  autoComplete="current-password"
                  value={password}
                  onChange={e => { setPassword(e.target.value); setError('') }}
                  required
                />
                <button
                  type="button"
                  className="login-pass-toggle"
                  onClick={() => setShowPass(v => !v)}
                  tabIndex={-1}
                  aria-label={showPass ? 'Hide password' : 'Show password'}
                >
                  <i className={`bi ${showPass ? 'bi-eye-slash' : 'bi-eye'}`} />
                </button>
              </div>
            </div>

            {/* Error message */}
            {error && (
              <div className="login-error-msg">
                <i className="bi bi-exclamation-circle-fill" />
                {error}
              </div>
            )}

            {/* Submit */}
            <button
              type="submit"
              className="login-submit-btn"
              disabled={loading || !email || !password}
            >
              {loading ? (
                <>
                  <span className="login-spinner" />
                  Signing in…
                </>
              ) : (
                <>
                  <i className="bi bi-arrow-right-circle-fill" />
                  Sign in
                </>
              )}
            </button>

          </form>

          {/* Note */}
          <p className="login-note">
            <i className="bi bi-info-circle me-1" />
            Don't have an account? Contact your hospital administrator
            to get access credentials.
          </p>

          <Link to="/" className="login-back-link">
            ← Back to patient portal
          </Link>
        </div>
      </div>
    </div>
  )
}

export default LoginPage
