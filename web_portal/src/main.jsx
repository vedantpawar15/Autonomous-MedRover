import React from 'react'
import ReactDOM from 'react-dom/client'
import { BrowserRouter } from 'react-router-dom'
import App from './App'

import 'bootstrap/dist/css/bootstrap.min.css'
import 'bootstrap/dist/js/bootstrap.bundle.min.js'
import 'bootstrap-icons/font/bootstrap-icons.min.css'
import './styles/style.css'

// ── Service Worker registration ───────────────────────────────────────────────
if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker
      .register('/sw.js', { scope: '/' })
      .then(reg => {
        console.log('[MedRover SW] Registered:', reg.scope)

        // Listen for Background Sync trigger messages from the SW
        navigator.serviceWorker.addEventListener('message', event => {
          if (event.data?.type === 'TRIGGER_SYNC') {
            // Dispatch a custom event so OfflineBanner can pick it up
            window.dispatchEvent(new CustomEvent('medrover-bg-sync'))
          }
        })
      })
      .catch(err => console.warn('[MedRover SW] Registration failed:', err))
  })
}

ReactDOM.createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <BrowserRouter>
      <App />
    </BrowserRouter>
  </React.StrictMode>
)
