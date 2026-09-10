import { createContext, useContext, useEffect, useState } from 'react'
import { supabase } from '../lib/supabaseClient'

// ─── Context ──────────────────────────────────────────────────────────────────
const AuthContext = createContext(null)

/**
 * AuthProvider — wraps the whole app.
 * Exposes: { user, session, loading, signIn, signOut, signUp }
 *
 * - `loading` is true only during the initial session check on mount.
 * - `user` is the Supabase User object, or null when logged out.
 * - `signIn` calls supabase.auth.signInWithPassword, then checks approval status.
 * - `signUp` creates a new auth user + pending user_profiles row.
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

  /** Sign up a new user — creates auth account + pending user_profiles row. */
  const signUp = async ({ email, password, fullName }) => {
    if (!supabase) return { error: { message: 'Supabase not configured.' } }
    const { data, error } = await supabase.auth.signUp({
      email,
      password,
      options: { data: { full_name: fullName } },
    })
    if (error) return { error }
    // If Supabase email confirmation is disabled, sign the new user out immediately
    // so they must wait for admin approval before being able to log in.
    if (data?.session) {
      await supabase.auth.signOut()
    }
    return { error: null }
  }

  /** Sign in — then verify the user has been approved by an admin. */
  const signIn = async ({ email, password }) => {
    if (!supabase) return { error: { message: 'Supabase not configured.' } }

    const { data: authData, error } = await supabase.auth.signInWithPassword({ email, password })
    if (error) return { error }

    const userId = authData?.user?.id
    if (!userId) return { error: null } // safe fallback

    // Check approval status for THIS specific user only.
    // Using maybeSingle() so pre-existing users with no profile row
    // (e.g. admin created before the trigger) get null instead of a 400 error.
    const { data: profile } = await supabase
      .from('user_profiles')
      .select('approval_status')
      .eq('id', userId)
      .maybeSingle()

    // If profile is null → user existed before the trigger → allow login
    if (profile?.approval_status === 'pending') {
      await supabase.auth.signOut()
      return { error: { message: '__PENDING__' } }
    }
    if (profile?.approval_status === 'rejected') {
      await supabase.auth.signOut()
      return { error: { message: '__REJECTED__' } }
    }

    return { error: null }
  }

  const signOut = async () => {
    if (!supabase) return
    await supabase.auth.signOut()
  }

  return (
    <AuthContext.Provider value={{ user, session, loading, signIn, signOut, signUp }}>
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
