import { createClient } from '@supabase/supabase-js'

// Supabase credentials for Autonomous MedRover frontend
// NOTE:
// - Reads values from Vite env variables (defined in .env).
// - Uses ONLY the public ANON key (safe for frontend usage).
// - Never put the service_role key in frontend code or env.

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL
const supabaseAnonKey = import.meta.env.VITE_SUPABASE_ANON_KEY

let supabase = null

if (!supabaseUrl || !supabaseAnonKey) {
  // This helps you quickly see if env vars are missing in development.
  // In production you should configure env properly.
  console.warn(
    'Supabase env variables are missing. Please set VITE_SUPABASE_URL and VITE_SUPABASE_ANON_KEY in your .env file.'
  )
} else {
  supabase = createClient(supabaseUrl, supabaseAnonKey)
}

export { supabase }


