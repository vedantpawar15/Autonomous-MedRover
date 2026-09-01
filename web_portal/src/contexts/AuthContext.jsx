import { createContext, useContext, useEffect, useState } from 'react'
import { supabase } from '../lib/supabaseClient'

// ─── Context ──────────────────────────────────────────────────────────────────
const AuthContext = createContext(null)

/**
 * AuthProvider — wraps the whole app.
 * Exposes: { user, session, loading, signIn, signOut }
 *
 * - `loading` is true only during the initial session check on mount.
 * - `user` is the Supabase User object, or null when logged out.
 * - `signIn` calls supabase.auth.signInWithPassword and returns { error }.
 * - `signOut` calls supabase.auth.signOut.
 */
export function AuthProvider({ children }) {
  const [user,    setUser]    = useState(null)
  const [session, setSession] = useState(null)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    if (!supabase) {
      setLoading(false)
      return
    }

    // 1. Check if there's an existing session on mount (e.g. page refresh)
    supabase.auth.getSession().then(({ data: { session } }) => {
      setSession(session)
      setUser(session?.user ?? null)
      setLoading(false)
    })

    // 2. Subscribe to auth state changes (login, logout, token refresh)
    const { data: { subscription } } = supabase.auth.onAuthStateChange(
      (_event, session) => {
        setSession(session)
        setUser(session?.user ?? null)
      }
    )

    return () => subscription.unsubscribe()
  }, [])

  // ── Auth actions ────────────────────────────────────────────────────────────
  const signIn = async ({ email, password }) => {
    if (!supabase) return { error: { message: 'Supabase not configured.' } }
    const { error } = await supabase.auth.signInWithPassword({ email, password })
    return { error }
  }

  const signOut = async () => {
    if (!supabase) return
    await supabase.auth.signOut()
  }

  return (
    <AuthContext.Provider value={{ user, session, loading, signIn, signOut }}>
      {children}
    </AuthContext.Provider>
  )
}

/**
 * useAuth — consume auth context anywhere in the app.
 * Must be used inside <AuthProvider>.
 */
export function useAuth() {
  const ctx = useContext(AuthContext)
  if (!ctx) throw new Error('useAuth must be used inside <AuthProvider>')
  return ctx
}
