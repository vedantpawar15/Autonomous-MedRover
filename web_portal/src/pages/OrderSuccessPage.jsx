import { Link } from 'react-router-dom'
import Navbar from '../components/Navbar'
import Footer from '../components/Footer'

function OrderSuccessPage() {
  return (
    <>
      <Navbar variant="inner" cartCount={0} />

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
                <Link to="/" className="btn-delivery-room mb-2">
                  <span>Back to Home</span>
                  <i className="bi bi-arrow-right-circle-fill"></i>
                </Link>
                <p className="small text-muted mt-2">
                  You can safely close this page. The robot will continue delivery automatically.
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


