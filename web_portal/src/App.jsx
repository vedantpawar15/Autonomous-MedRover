import { Routes, Route } from 'react-router-dom'
import { AuthProvider } from './contexts/AuthContext'
import ProtectedRoute from './components/ProtectedRoute'

import HomePage         from './pages/HomePage'
import SearchPage       from './pages/SearchPage'
import CartPage         from './pages/CartPage'
import SelectRoomPage   from './pages/SelectRoomPage'
import OrderSuccessPage from './pages/OrderSuccessPage'
import OrdersPage       from './pages/OrdersPage'
import AdminPage        from './pages/AdminPage'
import LoginPage        from './pages/LoginPage'

function App() {
  return (
    <AuthProvider>
      <Routes>
        {/* ── Public routes ─────────────────────────────────── */}
        <Route path="/"      element={<HomePage />}   />
        <Route path="/search" element={<SearchPage />} />
        <Route path="/login"  element={<LoginPage />}  />

        {/* ── Protected routes — require login ──────────────── */}
        <Route path="/admin" element={
          <ProtectedRoute><AdminPage /></ProtectedRoute>
        } />
        <Route path="/cart" element={
          <ProtectedRoute><CartPage /></ProtectedRoute>
        } />
        <Route path="/select-room" element={
          <ProtectedRoute><SelectRoomPage /></ProtectedRoute>
        } />
        <Route path="/order-success" element={
          <ProtectedRoute><OrderSuccessPage /></ProtectedRoute>
        } />
        <Route path="/orders" element={
          <ProtectedRoute><OrdersPage /></ProtectedRoute>
        } />
      </Routes>
    </AuthProvider>
  )
}

export default App
