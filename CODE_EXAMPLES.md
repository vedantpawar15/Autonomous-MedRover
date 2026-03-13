# Code Examples for MedRover Project

Quick reference code snippets you can copy and adapt for your project.

---

## 🛒 Cart Management Examples

### 1. Add to Cart (SearchPage.jsx)
```javascript
import { useState } from 'react'

function SearchPage() {
  const [cart, setCart] = useState([])

  const addToCart = (medicine) => {
    // Check if already in cart
    const existingItem = cart.find(item => item.id === medicine.id)
    
    if (existingItem) {
      // Update quantity
      setCart(cart.map(item =>
        item.id === medicine.id
          ? { ...item, qty: item.qty + 1 }
          : item
      ))
    } else {
      // Add new item
      setCart([...cart, { ...medicine, qty: 1 }])
    }
  }

  return (
    <button 
      className="btn-add-cart"
      onClick={() => addToCart(med)}
    >
      Add To Cart
    </button>
  )
}
```

### 2. Remove from Cart (CartPage.jsx)
```javascript
const removeFromCart = (id) => {
  setCartItems(cartItems.filter(item => item.id !== id))
}

// In JSX:
<button 
  className="cart-item-delete"
  onClick={() => removeFromCart(item.id)}
>
  <i className="bi bi-trash3"></i>
</button>
```

### 3. Update Quantity (CartPage.jsx)
```javascript
const updateQuantity = (id, newQty) => {
  setCartItems(cartItems.map(item =>
    item.id === id ? { ...item, selectedQty: parseInt(newQty) } : item
  ))
}

// In JSX:
<select 
  className="qty-select"
  value={item.selectedQty}
  onChange={(e) => updateQuantity(item.id, e.target.value)}
>
  {[1, 2, 3, 4, 5].map((n) => (
    <option key={n} value={n}>Qty {n}</option>
  ))}
</select>
```

### 4. Calculate Cart Total
```javascript
import { useEffect } from 'react'

const [total, setTotal] = useState(0)

useEffect(() => {
  const calculateTotal = () => {
    const sum = cartItems.reduce((acc, item) => {
      return acc + (item.currentPrice * item.selectedQty)
    }, 0)
    setTotal(sum.toFixed(2))
  }
  
  calculateTotal()
}, [cartItems])

// Display:
<span className="cart-total-amount">₹{total}</span>
```

### 5. Save Cart to localStorage
```javascript
// Save cart whenever it changes
useEffect(() => {
  localStorage.setItem('medrover_cart', JSON.stringify(cartItems))
}, [cartItems])

// Load cart on page load
useEffect(() => {
  const savedCart = localStorage.getItem('medrover_cart')
  if (savedCart) {
    try {
      setCartItems(JSON.parse(savedCart))
    } catch (error) {
      console.error('Error loading cart:', error)
    }
  }
}, [])
```

---

## 🔍 Search Functionality

### 1. Debounced Search
```javascript
import { useState, useEffect } from 'react'

const [searchQuery, setSearchQuery] = useState('')
const [filteredMedicines, setFilteredMedicines] = useState(medicines)

useEffect(() => {
  const timer = setTimeout(() => {
    if (searchQuery.trim()) {
      const filtered = medicines.filter(med =>
        med.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        med.brand.toLowerCase().includes(searchQuery.toLowerCase())
      )
      setFilteredMedicines(filtered)
    } else {
      setFilteredMedicines(medicines)
    }
  }, 300) // Wait 300ms after user stops typing

  return () => clearTimeout(timer) // Cleanup
}, [searchQuery])

// In JSX:
<input
  type="text"
  value={searchQuery}
  onChange={(e) => setSearchQuery(e.target.value)}
  placeholder="Search for Medicines..."
/>
```

---

## 🗄️ Supabase Integration

### 1. Setup Supabase Client
```javascript
// Create: src/lib/supabaseClient.js
import { createClient } from '@supabase/supabase-js'

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL
const supabaseKey = import.meta.env.VITE_SUPABASE_ANON_KEY

export const supabase = createClient(supabaseUrl, supabaseKey)
```

### 2. Fetch Medicines from Supabase
```javascript
import { useState, useEffect } from 'react'
import { supabase } from '../lib/supabaseClient'

function SearchPage() {
  const [medicines, setMedicines] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)

  useEffect(() => {
    const fetchMedicines = async () => {
      try {
        setLoading(true)
        const { data, error } = await supabase
          .from('medicines')
          .select('*')
          .order('name')
        
        if (error) throw error
        setMedicines(data || [])
      } catch (err) {
        setError(err.message)
        console.error('Error fetching medicines:', err)
      } finally {
        setLoading(false)
      }
    }

    fetchMedicines()
  }, [])

  if (loading) return <div>Loading medicines...</div>
  if (error) return <div>Error: {error}</div>

  return (
    // Your JSX here
  )
}
```

### 3. Search Medicines in Supabase
```javascript
const searchMedicines = async (query) => {
  try {
    const { data, error } = await supabase
      .from('medicines')
      .select('*')
      .ilike('name', `%${query}%`)
      .limit(20)
    
    if (error) throw error
    return data
  } catch (error) {
    console.error('Search error:', error)
    return []
  }
}
```

### 4. Create Order in Supabase
```javascript
const createOrder = async (orderData) => {
  try {
    const { data, error } = await supabase
      .from('orders')
      .insert([{
        medicines: orderData.cartItems,
        delivery_room: orderData.selectedRoom,
        total_amount: orderData.total,
        status: 'pending',
        created_at: new Date().toISOString()
      }])
      .select()
      .single()
    
    if (error) throw error
    return data
  } catch (error) {
    console.error('Error creating order:', error)
    throw error
  }
}

// Usage:
const handlePlaceOrder = async () => {
  try {
    setLoading(true)
    const order = await createOrder({
      cartItems,
      selectedRoom,
      total: calculateTotal()
    })
    
    // Clear cart
    setCartItems([])
    localStorage.removeItem('medrover_cart')
    
    // Navigate to success page or show message
    alert(`Order placed! Order ID: ${order.id}`)
    navigate('/')
  } catch (error) {
    alert('Failed to place order. Please try again.')
  } finally {
    setLoading(false)
  }
}
```

### 5. Real-time Order Status Updates
```javascript
import { useEffect } from 'react'
import { supabase } from '../lib/supabaseClient'

function OrderTracking() {
  const [orderStatus, setOrderStatus] = useState('pending')

  useEffect(() => {
    const channel = supabase
      .channel('order-updates')
      .on('postgres_changes',
        {
          event: 'UPDATE',
          schema: 'public',
          table: 'orders',
          filter: `id=eq.${orderId}`
        },
        (payload) => {
          setOrderStatus(payload.new.status)
          console.log('Order status updated:', payload.new.status)
        }
      )
      .subscribe()

    return () => {
      supabase.removeChannel(channel)
    }
  }, [orderId])

  return (
    <div>
      <p>Status: {orderStatus}</p>
      {orderStatus === 'pending' && <span>⏳ Waiting for robot...</span>}
      {orderStatus === 'in_transit' && <span>🚚 Robot is delivering...</span>}
      {orderStatus === 'delivered' && <span>✅ Delivered!</span>}
    </div>
  )
}
```

---

## ✅ Form Validation

### 1. Room Selection Validation
```javascript
const [selectedRoom, setSelectedRoom] = useState('')
const [error, setError] = useState('')

const handlePlaceOrder = () => {
  // Reset error
  setError('')

  // Validate room selection
  if (!selectedRoom) {
    setError('Please select a delivery room')
    return
  }

  // Validate cart is not empty
  if (cartItems.length === 0) {
    setError('Your cart is empty')
    return
  }

  // Check prescription requirements
  const hasPrescriptionMedicines = cartItems.some(item => item.rx)
  if (hasPrescriptionMedicines && !hasPrescription) {
    setError('Prescription required for some medicines in your cart')
    return
  }

  // All validations passed
  createOrder()
}

// Display error:
{error && (
  <div className="alert alert-danger" role="alert">
    {error}
  </div>
)}
```

---

## ⏳ Loading States

### 1. Button Loading State
```javascript
const [loading, setLoading] = useState(false)

const handlePlaceOrder = async () => {
  setLoading(true)
  try {
    await createOrder()
    // Success
  } catch (error) {
    // Error handling
  } finally {
    setLoading(false)
  }
}

// In JSX:
<button 
  className="btn-place-order-final"
  onClick={handlePlaceOrder}
  disabled={loading}
>
  {loading ? (
    <>
      <span className="spinner-border spinner-border-sm me-2"></span>
      Placing Order...
    </>
  ) : (
    <>
      <i className="bi bi-send-fill me-2"></i>
      Place Order
    </>
  )}
</button>
```

### 2. Page Loading State
```javascript
const [loading, setLoading] = useState(true)

useEffect(() => {
  const loadData = async () => {
    setLoading(true)
    // Fetch data
    setLoading(false)
  }
  loadData()
}, [])

if (loading) {
  return (
    <div className="text-center py-5">
      <div className="spinner-border text-primary" role="status">
        <span className="visually-hidden">Loading...</span>
      </div>
      <p className="mt-3">Loading medicines...</p>
    </div>
  )
}
```

---

## 🎯 Global Cart Context (Advanced)

### 1. Create Cart Context
```javascript
// Create: src/context/CartContext.jsx
import { createContext, useContext, useState, useEffect } from 'react'

const CartContext = createContext()

export const CartProvider = ({ children }) => {
  const [cartItems, setCartItems] = useState([])

  // Load from localStorage
  useEffect(() => {
    const saved = localStorage.getItem('medrover_cart')
    if (saved) {
      setCartItems(JSON.parse(saved))
    }
  }, [])

  // Save to localStorage
  useEffect(() => {
    localStorage.setItem('medrover_cart', JSON.stringify(cartItems))
  }, [cartItems])

  const addToCart = (medicine) => {
    const existing = cartItems.find(item => item.id === medicine.id)
    if (existing) {
      setCartItems(cartItems.map(item =>
        item.id === medicine.id
          ? { ...item, qty: item.qty + 1 }
          : item
      ))
    } else {
      setCartItems([...cartItems, { ...medicine, qty: 1 }])
    }
  }

  const removeFromCart = (id) => {
    setCartItems(cartItems.filter(item => item.id !== id))
  }

  const updateQuantity = (id, qty) => {
    setCartItems(cartItems.map(item =>
      item.id === id ? { ...item, qty: parseInt(qty) } : item
    ))
  }

  const getCartCount = () => {
    return cartItems.reduce((sum, item) => sum + item.qty, 0)
  }

  const getCartTotal = () => {
    return cartItems.reduce((sum, item) => sum + (item.price * item.qty), 0)
  }

  const clearCart = () => {
    setCartItems([])
    localStorage.removeItem('medrover_cart')
  }

  return (
    <CartContext.Provider value={{
      cartItems,
      addToCart,
      removeFromCart,
      updateQuantity,
      getCartCount,
      getCartTotal,
      clearCart
    }}>
      {children}
    </CartContext.Provider>
  )
}

export const useCart = () => {
  const context = useContext(CartContext)
  if (!context) {
    throw new Error('useCart must be used within CartProvider')
  }
  return context
}
```

### 2. Wrap App with Provider
```javascript
// In App.jsx
import { CartProvider } from './context/CartContext'

function App() {
  return (
    <CartProvider>
      <Router>
        <Navbar />
        <Routes>
          {/* Your routes */}
        </Routes>
        <Footer />
      </Router>
    </CartProvider>
  )
}
```

### 3. Use Cart in Components
```javascript
// In SearchPage.jsx
import { useCart } from '../context/CartContext'

function SearchPage() {
  const { addToCart, getCartCount } = useCart()

  return (
    <>
      <button onClick={() => addToCart(med)}>Add To Cart</button>
      <p>Items in cart: {getCartCount()}</p>
    </>
  )
}

// In Navbar.jsx
import { useCart } from '../context/CartContext'

function Navbar() {
  const { getCartCount } = useCart()
  const cartCount = getCartCount()

  return (
    <Link to="/cart" className="nav-top-action nav-cart-action">
      <i className="bi bi-cart3"></i>
      <span>Cart</span>
      {cartCount > 0 && <span className="cart-badge">{cartCount}</span>}
    </Link>
  )
}
```

---

## 📝 Environment Variables

### 1. Create .env file
```bash
# In web_portal/.env
VITE_SUPABASE_URL=your_supabase_project_url
VITE_SUPABASE_ANON_KEY=your_supabase_anon_key
```

### 2. Add .env to .gitignore
```gitignore
# Already in your .gitignore, but verify:
.env
.env.local
.env*.local
```

---

## 🎨 Utility Functions

### 1. Format Currency
```javascript
// src/utils/format.js
export const formatCurrency = (amount) => {
  return `₹${parseFloat(amount).toFixed(2)}`
}

// Usage:
<span>{formatCurrency(item.price)}</span>
```

### 2. Calculate Discount
```javascript
export const calculateDiscount = (original, current) => {
  const discount = ((original - current) / original) * 100
  return Math.round(discount)
}

// Usage:
<span>{calculateDiscount(med.original, med.price)}% OFF</span>
```

---

## 🚨 Error Handling Pattern

```javascript
const [error, setError] = useState(null)
const [success, setSuccess] = useState(null)

const handleAction = async () => {
  try {
    setError(null)
    setSuccess(null)
    
    // Your action here
    await someAsyncOperation()
    
    setSuccess('Operation completed successfully!')
  } catch (err) {
    setError(err.message || 'Something went wrong')
    console.error('Error:', err)
  }
}

// In JSX:
{error && (
  <div className="alert alert-danger alert-dismissible fade show" role="alert">
    {error}
    <button 
      type="button" 
      className="btn-close" 
      onClick={() => setError(null)}
    ></button>
  </div>
)}

{success && (
  <div className="alert alert-success alert-dismissible fade show" role="alert">
    {success}
    <button 
      type="button" 
      className="btn-close" 
      onClick={() => setSuccess(null)}
    ></button>
  </div>
)}
```

---

## 📦 Required NPM Packages

```bash
# Install Supabase client
npm install @supabase/supabase-js

# Already installed:
# - react, react-dom
# - react-router-dom
# - bootstrap, bootstrap-icons
```

---

**Tip:** Copy these examples and adapt them to your project structure. Start with cart management, then move to Supabase integration!

