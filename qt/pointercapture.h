/* === mouse capture for the Qt port ===
 * Turns host pointer movement into relative PC-98 mouse movement while the
 * pointer is captured. Three ways of doing it, picked at capture time:
 *   Warp   - X11: the pointer is moved back to the centre so it never reaches
 *            the edge of the window.
 *   Plain  - movement between consecutive positions, no warping. Used where
 *            warping is refused (Wayland until pointer locking is in) or found
 *            not to work; the pointer then stops at the window edge.
 */

#ifndef NP2_QT_POINTERCAPTURE_H
#define NP2_QT_POINTERCAPTURE_H

#include <QObject>
#include <QPointF>

class QWidget;

class PointerCapture : public QObject
{
	Q_OBJECT

public:
	explicit PointerCapture(QWidget *target);

	bool isCaptured() const { return m_captured; }
	void capture();
	void release();
	void toggle();

	/* Feed every mouse move of the target widget, in widget coordinates */
	void onMouseMove(const QPointF &pos);

Q_SIGNALS:
	void capturedChanged(bool captured);

private:
	enum class Mode { Warp, Plain };

	QWidget *m_target;
	Mode     m_mode;
	bool     m_captured;
	bool     m_warpWorks;

	bool     m_lastValid;
	QPointF  m_last;
	bool     m_warpPending;
	int      m_warpWait;
	QPointF  m_remainder;

	QPointF centre() const;
	void warpToCentre();
	void emitDelta(const QPointF &delta);
};

#endif /* NP2_QT_POINTERCAPTURE_H */
