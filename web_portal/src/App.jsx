import { Routes, Route } from 'react-router-dom'
import HomePage from './pages/HomePage'
import SearchPage from './pages/SearchPage'
import CartPage from './pages/CartPage'
import SelectRoomPage from './pages/SelectRoomPage'
import OrderSuccessPage from './pages/OrderSuccessPage'
import OrdersPage from './pages/OrdersPage'

function App() {
  return (
    <Routes>
      <Route path="/" element={<HomePage />} />
      <Route path="/search" element={<SearchPage />} />
      <Route path="/cart" element={<CartPage />} />
      <Route path="/select-room" element={<SelectRoomPage />} />
      <Route path="/order-success" element={<OrderSuccessPage />} />
      <Route path="/orders" element={<OrdersPage />} />
    </Routes>
  )
}

export default App

