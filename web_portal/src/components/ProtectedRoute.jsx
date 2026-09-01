import { Navigate, useLocation } from 'react-router-dom'
import { useAuth } from '../contexts/AuthContext'

/**
 * ProtectedRoute — gates any route behind authentication.
 *
 * Usage in App.jsx:
 *   <Route path="/cart" element={<ProtectedRoute><CartPage /></ProtectedRoute>} />
 *
 * Behaviour:
 *  - While session is being checked (loading): show a minimal full-screen spinner
 *  - If user is null (not logged in): redirect to /login, preserving the
 *    intended destination so we can redirect back after login.
 *  - If user exists: render children normally.
 */
function ProtectedRoute({ children }) {
  const { user, loading } = useAuth()
  const location = useLocation()

  // Still resolving the session from Supabase — show spinner
  if (loading) {
    return (
      <div style={{
        position: 'fixed', inset: 0,
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        background: '#f4f6f9',
      }}>
        <div style={{
          width: 36, height: 36,
          border: '3.5px solid #e4e9ef',
          borderTopColor: '#1E7F78',
          borderRadius: '50%',
          animation: 'spin 0.7s linear infinite',
        }} />
        <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
      </div>
    )
  }

  // Not authenticated — send to login with the intended route saved in state
  if (!user) {
    return <Navigate to="/login" state={{ from: location }} replace />
  }

  return children
}

export default ProtectedRoute
