import { createClient } from '@supabase/supabase-js'

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL || 'https://tdkccbbqktzeojgeuaoh.supabase.co'
const supabaseAnonKey = import.meta.env.VITE_SUPABASE_ANON_KEY || 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InRka2NjYmJxa3R6ZW9qZ2V1YW9oIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMzMzUyODEsImV4cCI6MjA4ODkxMTI4MX0.eBjZ0U9FEIg3vwHqXr8KR8VygMQDORBcaRwVY1LsdyY'

export const supabase = createClient(supabaseUrl, supabaseAnonKey)
