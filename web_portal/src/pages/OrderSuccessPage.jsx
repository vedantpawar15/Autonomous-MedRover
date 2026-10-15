import { Link } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'

function OrderSuccessPage() {
  return (
    <>
      <Navbar variant="plain" cartCount={0} />

      <section className="select-room-section">
        <div className="container">
          <div className="row justify-content-center">
            <div className="col-lg-8">
              <div className="sidebar-card order-summary-card text-center">
                <div className="mb-3">
                  <i className="bi bi-check-circle-fill" style={{ fontSize: '3rem', color: '#1E7F78' }}></i>
                </div>
                <h4 className="mb-2">Order Placed Successfully</h4>
                <p className="mb-4">
                  Your medicine order has been sent to <strong>MedRover</strong>. The robot will start its
                  delivery soon and you&apos;ll see the status update in the orders dashboard.
                </p>
                <div className="d-flex flex-column flex-sm-row justify-content-center gap-2 mb-3">
                  <a
                    href="http://localhost:5173"
                    target="_blank"
                    rel="noreferrer"
                    className="btn btn-primary d-inline-flex align-items-center justify-content-center gap-2 py-2 px-4 shadow-sm"
                    style={{ borderRadius: '10px', fontWeight: 600 }}
                  >
                    <i className="bi bi-broadcast" />
                    <span>Track MedRover in 3D (Uber-Style Live Twin)</span>
                  </a>
                  <Link to="/" className="btn btn-outline-secondary d-inline-flex align-items-center justify-content-center gap-1 py-2 px-3" style={{ borderRadius: '10px' }}>
                    <span>Back to Home</span>
                  </Link>
                </div>
                <p className="small text-muted mt-2">
                  The robot has received your order over Wi-Fi and is executing the delivery path autonomously.
                </p>
              </div>
            </div>
          </div>
        </div>
      </section>

      <Footer variant="simple" />
    </>
  )
}

export default OrderSuccessPage


