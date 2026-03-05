import { Link } from 'react-router-dom'

/**
 * Footer — Shared across all pages
 * Props:
 *  - variant: "full" | "simple"
 *    - "full" shows brand, links, tech stack, project info
 *    - "simple" shows only copyright line
 */
function Footer({ variant = 'simple' }) {
  if (variant === 'full') {
    return (
      <footer className="site-footer">
        <div className="container">
          <div className="row g-4">

            {/* Brand Column */}
            <div className="col-lg-4 col-md-6">
              <div className="footer-brand">
                <img src="/assets/logo/white.png" height="32" alt="Logo" />
                <span>Autonomous MedRover</span>
              </div>
              <p className="footer-desc">
                A web-based autonomous hospital medicine delivery system. Built as a Minor Project
                integrating IoT, robotics, and cloud technologies.
              </p>
            </div>

            {/* Quick Links */}
            <div className="col-lg-2 col-md-6">
              <h6 className="footer-heading">Quick Links</h6>
              <ul className="footer-links">
                <li><Link to="/">Home</Link></li>
                <li><a href="#features">Features</a></li>
                <li><a href="#how-it-works">How It Works</a></li>
                <li><a href="#faq">FAQ</a></li>
              </ul>
            </div>

            {/* Tech Stack */}
            <div className="col-lg-3 col-md-6">
              <h6 className="footer-heading">Tech Stack</h6>
              <ul className="footer-links">
                <li><a href="#">React + Vite + Bootstrap</a></li>
                <li><a href="#">Supabase (Database)</a></li>
                <li><a href="#">ESP32 (IoT Controller)</a></li>
                <li><a href="#">L298N + IR Sensors</a></li>
              </ul>
            </div>

            {/* Contact */}
            <div className="col-lg-3 col-md-6">
              <h6 className="footer-heading">Project Info</h6>
              <ul className="footer-links">
                <li><i className="bi bi-mortarboard me-2"></i>Minor Project 2</li>
                <li><i className="bi bi-hospital me-2"></i>Hospital Automation</li>
                <li><i className="bi bi-gear me-2"></i>IoT + Robotics</li>
              </ul>
            </div>

          </div>

          <hr className="footer-divider" />
          <p className="footer-copy text-center">
            &copy; 2026 Autonomous MedRover. All Rights Reserved.
          </p>
        </div>
      </footer>
    )
  }

  // Simple footer
  return (
    <footer className="site-footer">
      <div className="container">
        <hr className="footer-divider" style={{ marginTop: 0 }} />
        <p className="footer-copy text-center">
          &copy; 2026 Autonomous MedRover. All Rights Reserved.
        </p>
      </div>
    </footer>
  )
}

export default Footer

