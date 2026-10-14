/* === main window for the Qt port === */

#ifndef NP2_QT_MAINWINDOW_H
#define NP2_QT_MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

class QAction;
class QActionGroup;
class QMenu;
class ScreenWidget;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow();
	~MainWindow() override;

	ScreenWidget *screen() const { return m_screen; }

	/* Pause and resume around anything modal, as the wx port does */
	void stopEmulation();
	void startEmulation();

public Q_SLOTS:
	void refreshScreen();
	void updateCaption(int flag);
	void updateMenuStatus();
	void toggleFullScreen();
	void toggleMenuBar();

protected:
	void closeEvent(QCloseEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	ScreenWidget *m_screen;
	QTimer        m_emuTimer;
	bool          m_running;

	QAction *m_actStartPause;
	QAction *m_actFullScreen;
	QAction *m_actRotate[4];
	QMenu   *m_fdMenu[4];
	QMenu   *m_hdMenu[4];
	QMenu   *m_cdMenu;

	void buildMenus();
	void onEmuTimer();
	void rememberGeometry();

	void startPause();
	void reset();
	void openFd(int drv);
	void ejectFd(int drv);
	void openHd(int drv);
	void ejectHd(int drv);
	void openCd();
	void ejectCd();
	void sendCtrlAltDel();
	void setRotation(int index);
	void setScreenScale(int scale);
};

#endif /* NP2_QT_MAINWINDOW_H */
