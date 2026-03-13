# JavaScript & React Learning Guide for MedRover Project

## 📚 Learning Path Overview

This guide is tailored specifically for implementing JavaScript functionality in your **Autonomous MedRover** React project. Learn in this order for best results.

---

## 🎯 **PHASE 1: Core JavaScript Fundamentals** (Week 1-2)

### 1.1 Variables & Data Types
**What to learn:**
- `let`, `const`, `var` (use `const` and `let` only)
- Primitive types: `string`, `number`, `boolean`, `null`, `undefined`
- Objects and Arrays
- Template literals (backticks `` `Hello ${name}` ``)

**Practice:** Create variables for medicine data, cart items, user info

### 1.2 Functions
**What to learn:**
- Function declarations vs arrow functions
- Parameters and return values
- Default parameters
- Higher-order functions

**Example you'll use:**
```javascript
const calculateTotal = (items) => {
  return items.reduce((sum, item) => sum + item.price, 0)
}
```

### 1.3 Array Methods (CRITICAL for your project!)
**What to learn:**
- `.map()` - Transform arrays (you're already using this!)
- `.filter()` - Filter medicines by search query
- `.find()` - Find specific medicine by ID
- `.reduce()` - Calculate cart totals
- `.forEach()` - Loop through items
- `.includes()` - Check if item exists

**Practice:** Filter medicines, calculate cart totals, search functionality

### 1.4 Object Methods
**What to learn:**
- Object destructuring: `const { name, price } = medicine`
- Spread operator: `{...medicine, qty: 2}`
- Object.keys(), Object.values()
- Optional chaining: `medicine?.price`

**Example you'll use:**
```javascript
const addToCart = (medicine) => {
  setCart([...cart, { ...medicine, qty: 1 }])
}
```

### 1.5 Conditional Logic
**What to learn:**
- `if/else`, ternary operator (`condition ? true : false`)
- Logical operators (`&&`, `||`, `!`)
- Switch statements

**Example you'll use:**
```javascript
{medicine.inCart ? (
  <select>...</select>
) : (
  <button>Add To Cart</button>
)}
```

### 1.6 Async JavaScript (CRITICAL for Supabase!)
**What to learn:**
- Promises (`.then()`, `.catch()`)
- `async/await` syntax
- `fetch()` API for HTTP requests
- Error handling with `try/catch`

**Example you'll use:**
```javascript
const fetchMedicines = async () => {
  try {
    const response = await fetch('YOUR_SUPABASE_URL')
    const data = await response.json()
    return data
  } catch (error) {
    console.error('Error:', error)
  }
}
```

---

## ⚛️ **PHASE 2: React Fundamentals** (Week 2-3)

### 2.1 React Hooks (You're already using some!)

#### `useState` - State Management
**What to learn:**
- Creating state: `const [cart, setCart] = useState([])`
- Updating state: `setCart([...cart, newItem])`
- State immutability (never mutate directly!)

**What you need to implement:**
```javascript
// In CartPage.jsx
const [cartItems, setCartItems] = useState([])
const [total, setTotal] = useState(0)

// Add item to cart
const addToCart = (medicine) => {
  setCartItems([...cartItems, medicine])
}

// Remove item
const removeFromCart = (id) => {
  setCartItems(cartItems.filter(item => item.id !== id))
}

// Update quantity
const updateQuantity = (id, newQty) => {
  setCartItems(cartItems.map(item => 
    item.id === id ? { ...item, qty: newQty } : item
  ))
}
```

#### `useEffect` - Side Effects (CRITICAL for API calls!)
**What to learn:**
- Fetching data on component mount
- Dependency array `[]` vs `[dependency]`
- Cleanup functions

**What you need to implement:**
```javascript
// Fetch medicines from Supabase
useEffect(() => {
  const loadMedicines = async () => {
    const data = await fetchMedicines()
    setMedicines(data)
  }
  loadMedicines()
}, []) // Run once on mount

// Update cart total when cart changes
useEffect(() => {
  const newTotal = cartItems.reduce((sum, item) => 
    sum + (item.price * item.qty), 0
  )
  setTotal(newTotal)
}, [cartItems]) // Run when cartItems changes
```

#### `useContext` - Global State (For cart across pages)
**What to learn:**
- Creating context: `createContext()`
- Provider component
- `useContext()` hook

**What you need to implement:**
```javascript
// Create CartContext.jsx
const CartContext = createContext()

export const CartProvider = ({ children }) => {
  const [cart, setCart] = useState([])
  // ... cart functions
  return (
    <CartContext.Provider value={{ cart, addToCart, removeFromCart }}>
      {children}
    </CartContext.Provider>
  )
}

// Use in components
const { cart, addToCart } = useContext(CartContext)
```

### 2.2 Event Handling
**What to learn:**
- `onClick`, `onSubmit`, `onChange`
- Event object: `e.preventDefault()`, `e.target.value`
- Passing parameters to handlers

**Examples you'll use:**
```javascript
// Button click
<button onClick={() => addToCart(medicine)}>Add To Cart</button>

// Form submit
const handleSubmit = (e) => {
  e.preventDefault()
  // Process form
}

// Input change
<input onChange={(e) => setSearchQuery(e.target.value)} />
```

### 2.3 Conditional Rendering
**What to learn:**
- `{condition && <Component />}`
- Ternary operator
- Early returns

**You're already using this!** Keep practicing.

### 2.4 Lists & Keys
**What to learn:**
- `.map()` for rendering lists
- `key` prop (you're already using it!)
- Unique keys (use `id`, not index)

---

## 🔌 **PHASE 3: React Router** (Week 3) - You're already using this!

### What you know:
- `useNavigate()` - Navigate programmatically
- `useSearchParams()` - Get URL query parameters
- `Link` component - Navigation links

### What to learn more:
- `useLocation()` - Get current route
- `useParams()` - Get route parameters (if you add `/medicine/:id`)
- Protected routes (if you add authentication)

---

## 🗄️ **PHASE 4: Supabase Integration** (Week 4)

### 4.1 Supabase Client Setup
**What to learn:**
- Installing `@supabase/supabase-js`
- Creating Supabase client
- Environment variables (`.env` file)

**Implementation:**
```javascript
// supabaseClient.js
import { createClient } from '@supabase/supabase-js'

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL
const supabaseKey = import.meta.env.VITE_SUPABASE_ANON_KEY

export const supabase = createClient(supabaseUrl, supabaseKey)
```

### 4.2 Database Operations
**What to learn:**
- `SELECT` - Fetch medicines
- `INSERT` - Create orders
- `UPDATE` - Update order status
- `DELETE` - Remove items (optional)

**Examples you'll use:**
```javascript
// Fetch all medicines
const fetchMedicines = async () => {
  const { data, error } = await supabase
    .from('medicines')
    .select('*')
  
  if (error) throw error
  return data
}

// Search medicines
const searchMedicines = async (query) => {
  const { data, error } = await supabase
    .from('medicines')
    .select('*')
    .ilike('name', `%${query}%`)
  
  return data
}

// Create order
const createOrder = async (orderData) => {
  const { data, error } = await supabase
    .from('orders')
    .insert([orderData])
    .select()
  
  if (error) throw error
  return data
}

// Update order status (for robot)
const updateOrderStatus = async (orderId, status) => {
  const { data, error } = await supabase
    .from('orders')
    .update({ status })
    .eq('id', orderId)
  
  return data
}
```

### 4.3 Real-time Subscriptions (Advanced)
**What to learn:**
- `supabase.channel()` for real-time updates
- Listening to database changes
- Updating UI when robot updates order status

**Example:**
```javascript
useEffect(() => {
  const channel = supabase
    .channel('orders')
    .on('postgres_changes', 
      { event: 'UPDATE', schema: 'public', table: 'orders' },
      (payload) => {
        // Update order status in UI
        updateOrderInState(payload.new)
      }
    )
    .subscribe()

  return () => {
    supabase.removeChannel(channel)
  }
}, [])
```

---

## 🛒 **PHASE 5: Project-Specific Features** (Week 5)

### 5.1 Cart Management
**What to implement:**
- Add to cart (from search page)
- Remove from cart
- Update quantity
- Calculate totals
- Persist cart in localStorage

**Implementation:**
```javascript
// Save cart to localStorage
useEffect(() => {
  localStorage.setItem('cart', JSON.stringify(cartItems))
}, [cartItems])

// Load cart from localStorage
useEffect(() => {
  const savedCart = localStorage.getItem('cart')
  if (savedCart) {
    setCartItems(JSON.parse(savedCart))
  }
}, [])
```

### 5.2 Search Functionality
**What to implement:**
- Debounced search (wait for user to stop typing)
- Filter medicines by name/brand
- Highlight search terms

**Implementation:**
```javascript
const [searchQuery, setSearchQuery] = useState('')
const [filteredMedicines, setFilteredMedicines] = useState([])

useEffect(() => {
  const timer = setTimeout(() => {
    if (searchQuery) {
      const filtered = medicines.filter(med =>
        med.name.toLowerCase().includes(searchQuery.toLowerCase())
      )
      setFilteredMedicines(filtered)
    } else {
      setFilteredMedicines(medicines)
    }
  }, 300) // Wait 300ms after user stops typing

  return () => clearTimeout(timer)
}, [searchQuery])
```

### 5.3 Form Validation
**What to implement:**
- Validate room selection
- Validate prescription requirements
- Show error messages

**Example:**
```javascript
const handlePlaceOrder = () => {
  if (!selectedRoom) {
    setError('Please select a delivery room')
    return
  }
  
  if (hasPrescriptionMedicines && !hasPrescription) {
    setError('Prescription required for some medicines')
    return
  }
  
  // Proceed with order
}
```

### 5.4 Loading States
**What to implement:**
- Show loading spinner while fetching data
- Disable buttons during API calls
- Show "Loading..." messages

**Example:**
```javascript
const [loading, setLoading] = useState(false)

const handlePlaceOrder = async () => {
  setLoading(true)
  try {
    await createOrder(orderData)
    // Success
  } catch (error) {
    // Error handling
  } finally {
    setLoading(false)
  }
}

return (
  <button disabled={loading}>
    {loading ? 'Placing Order...' : 'Place Order'}
  </button>
)
```

### 5.5 Error Handling
**What to implement:**
- Try/catch blocks
- Error messages to users
- Fallback UI for errors

**Example:**
```javascript
const [error, setError] = useState(null)

try {
  const data = await fetchMedicines()
  setMedicines(data)
} catch (err) {
  setError('Failed to load medicines. Please try again.')
  console.error(err)
}

{error && (
  <div className="alert alert-danger">{error}</div>
)}
```

---

## 📦 **Recommended Learning Resources**

### Free Resources:
1. **MDN Web Docs** - Best JavaScript reference: https://developer.mozilla.org/en-US/docs/Web/JavaScript
2. **React Official Docs** - https://react.dev
3. **Supabase Docs** - https://supabase.com/docs
4. **JavaScript.info** - Comprehensive JS tutorial: https://javascript.info
5. **React Router Docs** - https://reactrouter.com

### Video Courses:
1. **freeCodeCamp** - Full React Course (YouTube)
2. **Traversy Media** - React Crash Course
3. **Net Ninja** - React & Supabase tutorials

### Practice:
1. **CodeSandbox** - Online React playground
2. **React.dev Learn** - Interactive tutorials
3. Build small projects: Todo app, Calculator, Weather app

---

## 🎯 **Priority Checklist for Your Project**

### Must Learn First (Week 1-2):
- [ ] JavaScript: Arrays, Objects, Functions
- [ ] React: `useState`, `useEffect`
- [ ] Event handling: `onClick`, `onSubmit`, `onChange`
- [ ] Form handling and validation

### Next Priority (Week 3):
- [ ] Supabase setup and basic queries
- [ ] `fetch()` API and async/await
- [ ] Error handling with try/catch
- [ ] Loading states

### Advanced Features (Week 4-5):
- [ ] `useContext` for global cart state
- [ ] localStorage for cart persistence
- [ ] Real-time updates with Supabase
- [ ] Debounced search

---

## 💡 **Quick Start: First JavaScript Feature to Implement**

Start with **Cart Management** - it's the most visible feature:

1. **Add to Cart button** (in SearchPage.jsx)
2. **Remove from Cart** (in CartPage.jsx)
3. **Update Quantity** (in CartPage.jsx)
4. **Calculate Total** (in CartPage.jsx)

This will teach you:
- `useState` for managing cart
- Event handlers (`onClick`, `onChange`)
- Array methods (`.filter()`, `.map()`, `.reduce()`)
- State updates

---

## 🚀 **Next Steps**

1. **Start with Phase 1** - Master JavaScript fundamentals (1-2 weeks)
2. **Move to Phase 2** - Learn React hooks (1 week)
3. **Practice** - Build a simple cart feature
4. **Phase 4** - Connect to Supabase (1 week)
5. **Phase 5** - Implement all project features

**Remember:** Practice by building! Don't just read - code along with tutorials and implement features in your project.

Good luck! 🎉

