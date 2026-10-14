/* === key display window for the Qt port ===
 * The keyboards of generic/keydisp.c, as the Windows and GTK ports show them:
 * one row per FM, PSG or MIDI channel, its playing notes lit up.
 */

#ifndef NP2_QT_KEYDISPWINDOW_H
#define NP2_QT_KEYDISPWINDOW_H

#include <QImage>
#include <QTimer>
#include <QWidget>

class QActionGroup;
class QMenuBar;
class MainWindow;

class KeyDispWindow : public QWidget
{
	Q_OBJECT

public:
	explicit KeyDispWindow(MainWindow *parent);
	~KeyDispWindow() override;

Q_SIGNALS:
	void closed();

protected:
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	MainWindow   *m_main;
	QMenuBar     *m_menuBar;
	QActionGroup *m_modes;
	QImage        m_image;
	QTimer        m_timer;

	void switchMode(unsigned char mode);
	void setMode(unsigned char mode);
	void tick();
	void resizeToKeys();
};

#endif /* NP2_QT_KEYDISPWINDOW_H */
