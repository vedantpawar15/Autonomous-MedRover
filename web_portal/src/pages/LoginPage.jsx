import { useState, useRef, useEffect } from 'react'
import { Link, useNavigate, useLocation } from 'react-router-dom'
import { useAuth } from '../contexts/AuthContext'

/**
 * LoginPage — supports both Sign In and Create Account tabs.
 * Sign Up creates a pending account; admin must approve before user can log in.
 */
function LoginPage() {
  const { user, signIn, signUp } = useAuth()
  const navigate                  = useNavigate()
  const location                  = useLocation()
  const emailRef                  = useRef(null)

  const [tab,        setTab]        = useState('login') // 'login' | 'signup'
  const [email,      setEmail]      = useState('')
  const [fullName,   setFullName]   = useState('')
  const [password,   setPassword]   = useState('')
  const [confirm,    setConfirm]    = useState('')
  const [showPass,   setShowPass]   = useState(false)
  const [showConf,   setShowConf]   = useState(false)
  const [error,      setError]      = useState('')
  const [success,    setSuccess]    = useState('')
  const [loading,    setLoading]    = useState(false)

  const from = location.state?.from?.pathname || '/'

  useEffect(() => { if (user) navigate(from, { replace: true }) }, [user, from, navigate])
  useEffect(() => { emailRef.current?.focus() }, [tab])

  const resetForm = () => {
    setEmail(''); setPassword(''); setFullName(''); setConfirm('')
    setError(''); setSuccess('')
  }

  const switchTab = (t) => { setTab(t); resetForm() }

  // ── Login handler ──────────────────────────────────────────────────────────
  const handleLogin = async (e) => {
    e.preventDefault()
    setError(''); setLoading(true)

    const { error: err } = await signIn({ email: email.trim(), password })
    setLoading(false)

    if (err) {
      if (err.message === '__PENDING__') {
        setError('⏳ Your account is awaiting admin approval. Please check back later.')
      } else if (err.message === '__REJECTED__') {
        setError('❌ Your account access has been rejected. Please contact the administrator.')
      } else if (
        err.message?.toLowerCase().includes('email not confirmed') ||
        err.message?.toLowerCase().includes('email_not_confirmed') ||
        err.code === 'email_not_confirmed'
      ) {
        setError('📧 Your email address is not yet confirmed. Please contact your hospital administrator.')
      } else if (
        err.message?.toLowerCase().includes('invalid login credentials') ||
        err.message?.toLowerCase().includes('invalid credentials')
      ) {
        setError('Incorrect email or password. Please try again.')
      } else {
        setError(err.message || 'Login failed. Please try again.')
      }
      return
    }

    navigate(from, { replace: true })
  }

  // ── Signup handler ─────────────────────────────────────────────────────────
  const handleSignup = async (e) => {
    e.preventDefault()
    setError('')

    if (password !== confirm) {
      setError('Passwords do not match.'); return
    }
    if (password.length < 6) {
      setError('Password must be at least 6 characters.'); return
    }

    setLoading(true)
    const { error: err } = await signUp({
      email: email.trim(),
      password,
      fullName: fullName.trim(),
    })
    setLoading(false)

    if (err) {
      if (err.message?.toLowerCase().includes('already registered') ||
          err.message?.toLowerCase().includes('user already registered')) {
        setError('This email is already registered. Please sign in instead.')
      } else {
        setError(err.message || 'Sign up failed. Please try again.')
      }
      return
    }

    setSuccess('✅ Account created! Your request is pending admin approval. You will be able to log in once approved.')
    setEmail(''); setPassword(''); setConfirm(''); setFullName('')
  }

  return (
    <div className="login-page">

      {/* ── Left branding panel ─────────────────────────────────── */}
      <div className="login-left">
        <div className="login-brand">
          <Link to="/" className="login-brand-link">
            <img src="/assets/logo/white.png" height="30" alt="MedRover" />
            <div>
              <div className="login-brand-name">MedRover</div>
              <div className="login-brand-tag">Hospital Delivery System</div>
            </div>
          </Link>
        </div>

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

          {/* Tab switcher */}
          <div className="login-tabs">
            <button
              className={`login-tab-btn${tab === 'login' ? ' active' : ''}`}
              onClick={() => switchTab('login')}
            >
              <i className="bi bi-box-arrow-in-right me-1" />
              Sign In
            </button>
            <button
              className={`login-tab-btn${tab === 'signup' ? ' active' : ''}`}
              onClick={() => switchTab('signup')}
            >
              <i className="bi bi-person-plus me-1" />
              Create Account
            </button>
          </div>

          {/* ─── SIGN IN FORM ─────────────────────────────────── */}
          {tab === 'login' && (
            <>
              <div className="login-form-eyebrow">Staff Portal</div>
              <h2 className="login-form-title">Sign in to your account</h2>
              <p className="login-form-sub">
                Use your approved staff credentials to access the portal.
              </p>

              <form onSubmit={handleLogin} noValidate>
                <div className="login-field">
                  <label className="login-field-label" htmlFor="loginEmail">Email address</label>
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

                <div className="login-field">
                  <label className="login-field-label" htmlFor="loginPassword">Password</label>
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

                {error && (
                  <div className="login-error-msg">
                    <i className="bi bi-exclamation-circle-fill" />
                    {error}
                  </div>
                )}

                <button
                  type="submit"
                  className="login-submit-btn"
                  disabled={loading || !email || !password}
                >
                  {loading ? (
                    <><span className="login-spinner" />Signing in…</>
                  ) : (
                    <><i className="bi bi-arrow-right-circle-fill" />Sign in</>
                  )}
                </button>
              </form>

              <p className="login-note">
                <i className="bi bi-info-circle me-1" />
                Don't have an account?{' '}
                <button className="login-note-link" onClick={() => switchTab('signup')}>
                  Create one here
                </button>{' '}
                — subject to admin approval.
              </p>
            </>
          )}

          {/* ─── SIGN UP FORM ─────────────────────────────────── */}
          {tab === 'signup' && (
            <>
              <div className="login-form-eyebrow">Staff Portal</div>
              <h2 className="login-form-title">Create an account</h2>
              <p className="login-form-sub">
                Submit your details. An admin will review and approve your access.
              </p>

              {success ? (
                <div className="login-success-msg">
                  <i className="bi bi-check-circle-fill" />
                  <div>
                    <strong>Request submitted!</strong>
                    <p style={{ margin: '4px 0 0', fontSize: '0.875rem' }}>
                      Your account is pending admin approval. You'll be able to sign in once approved.
                    </p>
                  </div>
                </div>
              ) : (
                <form onSubmit={handleSignup} noValidate>
                  <div className="login-field">
                    <label className="login-field-label" htmlFor="signupName">Full name</label>
                    <div className="login-input-wrap">
                      <i className="bi bi-person login-input-icon" />
                      <input
                        id="signupName"
                        ref={emailRef}
                        type="text"
                        className="login-input"
                        placeholder="Dr. Jane Smith"
                        autoComplete="name"
                        value={fullName}
                        onChange={e => { setFullName(e.target.value); setError('') }}
                        required
                      />
                    </div>
                  </div>

                  <div className="login-field">
                    <label className="login-field-label" htmlFor="signupEmail">Email address</label>
                    <div className={`login-input-wrap${error ? ' input-error' : ''}`}>
                      <i className="bi bi-envelope login-input-icon" />
                      <input
                        id="signupEmail"
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

                  <div className="login-field">
                    <label className="login-field-label" htmlFor="signupPassword">Password</label>
                    <div className={`login-input-wrap${error ? ' input-error' : ''}`}>
                      <i className="bi bi-key login-input-icon" />
                      <input
                        id="signupPassword"
                        type={showPass ? 'text' : 'password'}
                        className="login-input"
                        placeholder="Min. 6 characters"
                        autoComplete="new-password"
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

                  <div className="login-field">
                    <label className="login-field-label" htmlFor="signupConfirm">Confirm password</label>
                    <div className={`login-input-wrap${error ? ' input-error' : ''}`}>
                      <i className="bi bi-key-fill login-input-icon" />
                      <input
                        id="signupConfirm"
                        type={showConf ? 'text' : 'password'}
                        className="login-input"
                        placeholder="Re-enter password"
                        autoComplete="new-password"
                        value={confirm}
                        onChange={e => { setConfirm(e.target.value); setError('') }}
                        required
                      />
                      <button
                        type="button"
                        className="login-pass-toggle"
                        onClick={() => setShowConf(v => !v)}
                        tabIndex={-1}
                        aria-label={showConf ? 'Hide password' : 'Show password'}
                      >
                        <i className={`bi ${showConf ? 'bi-eye-slash' : 'bi-eye'}`} />
                      </button>
                    </div>
                  </div>

                  {error && (
                    <div className="login-error-msg">
                      <i className="bi bi-exclamation-circle-fill" />
                      {error}
                    </div>
                  )}

                  <button
                    type="submit"
                    className="login-submit-btn"
                    disabled={loading || !email || !password || !confirm || !fullName}
                  >
                    {loading ? (
                      <><span className="login-spinner" />Creating account…</>
                    ) : (
                      <><i className="bi bi-person-check-fill" />Request Access</>
                    )}
                  </button>
                </form>
              )}

              <p className="login-note">
                <i className="bi bi-info-circle me-1" />
                Already have an account?{' '}
                <button className="login-note-link" onClick={() => switchTab('login')}>
                  Sign in
                </button>
              </p>
            </>
          )}

          <Link to="/" className="login-back-link">
            ← Back to patient portal
          </Link>
        </div>
      </div>
    </div>
  )
}

export default LoginPage
