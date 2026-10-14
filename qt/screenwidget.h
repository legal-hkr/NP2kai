/* === emulated screen for the Qt port ===
 * Shows the frame from scrnmng and feeds keyboard and mouse to the core.
 */

#ifndef NP2_QT_SCREENWIDGET_H
#define NP2_QT_SCREENWIDGET_H

#include <QImage>
#include <QWidget>

class PointerCapture;

class ScreenWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ScreenWidget(QWidget *parent = nullptr);

	/* Copy the current frame from scrnmng; GUI thread only */
	void updateFrame();

	PointerCapture *pointerCapture() const { return m_capture; }

	/* The frame as shown (rotation applied), for screenshots */
	QImage frame() const;

	QSize sizeHint() const override;

Q_SIGNALS:
	void fullScreenToggleRequested();
	void menuBarToggleRequested();

protected:
	void paintEvent(QPaintEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
	bool focusNextPrevChild(bool next) override;

private:
	QImage          m_image;
	PointerCapture *m_capture;

	bool handleHotkey(QKeyEvent *event, bool pressed);
};

#endif /* NP2_QT_SCREENWIDGET_H */
