/* === mouse capture for the Qt port === */

#include <compiler.h>
#include "pointercapture.h"
#include "mousemng.h"

#include <QCursor>
#include <QGuiApplication>
#include <QWidget>

#include <cmath>

/* A warp shows up as a motion event right on the centre. This many events
 * without one and warping is taken not to work on this display. */
static const int kWarpWait = 8;

PointerCapture::PointerCapture(QWidget *target)
	: QObject(target)
	, m_target(target)
	, m_mode(Mode::Plain)
	, m_captured(false)
	, m_warpWorks(true)
	, m_lastValid(false)
	, m_warpPending(false)
	, m_warpWait(0)
{
}

QPointF PointerCapture::centre() const
{
	return QPointF(m_target->width() / 2.0, m_target->height() / 2.0);
}

void PointerCapture::capture()
{
	if (m_captured) {
		return;
	}
	/* Only X11 lets a client move the pointer; Wayland refuses by design. */
	m_mode = (QGuiApplication::platformName() == QLatin1String("xcb") && m_warpWorks)
	         ? Mode::Warp : Mode::Plain;

	m_captured = true;
	m_lastValid = false;   /* the first move sets the starting point */
	m_warpPending = false;
	m_remainder = QPointF();

	m_target->setMouseTracking(true);
	m_target->setCursor(Qt::BlankCursor);
	m_target->grabMouse();
	if (m_mode == Mode::Warp) {
		warpToCentre();
	}
	mousemng_hidecursor();
	Q_EMIT capturedChanged(true);
}

void PointerCapture::release()
{
	if (!m_captured) {
		return;
	}
	m_captured = false;
	m_target->releaseMouse();
	m_target->unsetCursor();
	m_target->setMouseTracking(false);
	mousemng_showcursor();
	Q_EMIT capturedChanged(false);
}

void PointerCapture::toggle()
{
	if (m_captured) {
		release();
	} else {
		capture();
	}
}

void PointerCapture::warpToCentre()
{
	QCursor::setPos(m_target->mapToGlobal(centre().toPoint()));
	m_warpPending = true;
	m_warpWait = kWarpWait;
}

void PointerCapture::onMouseMove(const QPointF &pos)
{
	if (!m_captured) {
		return;
	}
	const QPointF c = centre();

	/* Until the warp is seen, movement is measured from the last real
	 * position, so a warp still in flight or refused costs nothing. */
	if (m_warpPending) {
		if (std::abs(pos.x() - c.x()) < 1.0 && std::abs(pos.y() - c.y()) < 1.0) {
			m_warpPending = false;
			m_last = pos;
			m_lastValid = true;
			return;
		}
		if (--m_warpWait <= 0) {
			m_warpPending = false;
			m_warpWorks = false;
			m_mode = Mode::Plain;
		}
	}

	if (!m_lastValid) {
		m_last = pos;
		m_lastValid = true;
		return;
	}

	emitDelta(pos - m_last);
	m_last = pos;

	/* Back to the centre once outside the middle half of the widget */
	if (m_mode == Mode::Warp && !m_warpPending
	    && (std::abs(pos.x() - c.x()) > m_target->width() / 4.0
	        || std::abs(pos.y() - c.y()) > m_target->height() / 4.0)) {
		warpToCentre();
	}
}

/* Widget coordinates are logical; the PC-98 mouse moves in device pixels, as
 * it did in the wx port, so HiDPI scaling doesn't slow it down. Fractions are
 * carried over to the next event. */
void PointerCapture::emitDelta(const QPointF &delta)
{
	const QPointF d = delta * m_target->devicePixelRatioF() + m_remainder;
	const int dx = static_cast<int>(d.x());
	const int dy = static_cast<int>(d.y());
	m_remainder = QPointF(d.x() - dx, d.y() - dy);
	if (dx || dy) {
		mousemng_onmove(dx, dy);
	}
}
