/* === main window for the Qt port ===
 * Mirrors Np2Frame (wx/np2frame.cpp): owns the emulation timer or thread,
 * the menus and the hooks the shared wx code calls (wx/np2ui.h).
 */

#include <compiler.h>
#include "mainwindow.h"
#include "screenwidget.h"
#include "pointercapture.h"
#include "newdisk/diskcreator.h"
#include "prefs/prefdialog.h"
#include "np2ui.h"
#include "np2mt.h"
#include "np2.h"
#include "scrnmng.h"
#include "soundmng.h"
#include <pccore.h>
#include <keystat.h>
#include <sysmng.h>
#include <dosio.h>
#include <diskimage/fddfile.h>
#include <fdd/diskdrv.h>
#include <fdd/sxsi.h>
#include <io/iocore.h>
#include <wab/wab.h>

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>

#include <atomic>

static MainWindow *s_window = nullptr;

/* scrnmng_update() runs once per emulated frame on the emulator thread;
 * one queued repaint at a time is enough. */
static std::atomic<bool> s_redrawPending{false};

/* ---- hooks for the shared wx code (wx/np2ui.h) ---- */

extern "C" void np2frame_requestRedraw(void)
{
	if (s_window && !s_redrawPending.exchange(true)) {
		QMetaObject::invokeMethod(s_window, &MainWindow::refreshScreen, Qt::QueuedConnection);
	}
}

extern "C" void np2frame_updateCaption(UINT8 flag)
{
	if (s_window) {
		const int f = flag;
		QMetaObject::invokeMethod(s_window, [f]() {
			if (s_window) {
				s_window->updateCaption(f);
			}
		}, Qt::QueuedConnection);
	}
}

extern "C" void np2frame_setTitle(const char *title)
{
	if (s_window) {
		const QString t = QString::fromUtf8(title);
		QMetaObject::invokeMethod(s_window, [t]() {
			if (s_window) {
				s_window->setWindowTitle(t);
			}
		}, Qt::QueuedConnection);
	}
}

/* ---- window ---- */

MainWindow::MainWindow()
	: m_screen(new ScreenWidget(this))
	, m_running(true)
	, m_actStartPause(nullptr)
	, m_actFullScreen(nullptr)
	, m_cdMenu(nullptr)
{
	s_window = this;
	setCentralWidget(m_screen);
	buildMenus();

	connect(m_screen, &ScreenWidget::fullScreenToggleRequested, this, &MainWindow::toggleFullScreen);
	connect(m_screen, &ScreenWidget::menuBarToggleRequested, this, &MainWindow::toggleMenuBar);
	connect(&m_emuTimer, &QTimer::timeout, this, &MainWindow::onEmuTimer);

	/* Always initialised: it also sets up the nevent mutex */
	np2_multithread_initialize();
	if (np2wabcfg.multithread) {
		/* The emulator has its own thread; the timer only refreshes the UI */
		np2_multithread_start();
		m_emuTimer.start(50);
	} else {
		m_emuTimer.start(1);
	}

	m_screen->setFocus();
}

MainWindow::~MainWindow()
{
	np2_multithread_shutdown();
	s_window = nullptr;
}

void MainWindow::onEmuTimer()
{
	if (np2wabcfg.multithread) {
		updateMenuStatus();
	} else if (m_running) {
		np2_exec();
	}
}

void MainWindow::stopEmulation()
{
	soundmng_stop();
	np2_multithread_suspend();
}

void MainWindow::startEmulation()
{
	if (!m_running) {
		return;
	}
	np2_multithread_resume();
	soundmng_play();
}

void MainWindow::refreshScreen()
{
	s_redrawPending = false;
	m_screen->updateFrame();
}

void MainWindow::updateCaption(int flag)
{
	sysmng_updatecaption(static_cast<UINT8>(flag));
	updateMenuStatus();
}

/* ---- menus ---- */

/* No menu shortcuts: every key, F5 and Ctrl+V included, belongs to the PC-98
 * while the screen has focus. Alt+Enter, F11 and F12 are handled by the
 * screen widget according to the F11KEY/F12KEY settings. */
void MainWindow::buildMenus()
{
	QMenu *emu = menuBar()->addMenu(tr("&Emulate"));
	m_actStartPause = emu->addAction(tr("&Pause"), this, &MainWindow::startPause);
	emu->addAction(tr("&Reset"), this, &MainWindow::reset);
	emu->addSeparator();
	emu->addAction(tr("New &FD Image..."), this, [this]() { DiskCreator::newFdImage(this); });
	emu->addAction(tr("New &HD Image..."), this, [this]() { DiskCreator::newHdImage(this); });
	emu->addSeparator();
	emu->addAction(tr("Pr&eferences..."), this, &MainWindow::openPreferences);
	emu->addSeparator();
	emu->addAction(tr("E&xit"), this, &QWidget::close);

	QMenu *fdd = menuBar()->addMenu(tr("&FDD"));
	for (int drv = 0; drv < 4; drv++) {
		m_fdMenu[drv] = fdd->addMenu(tr("FD &%1").arg(drv + 1));
		m_fdMenu[drv]->addAction(tr("&Open/Change Image..."), this, [this, drv]() { openFd(drv); });
		m_fdMenu[drv]->addAction(tr("&Eject"), this, [this, drv]() { ejectFd(drv); });
	}

	QMenu *hdd = menuBar()->addMenu(tr("&HDD"));
	for (int drv = 0; drv < 4; drv++) {
		m_hdMenu[drv] = hdd->addMenu(tr("HD &%1").arg(drv + 1));
		m_hdMenu[drv]->addAction(tr("&Open/Change Image..."), this, [this, drv]() { openHd(drv); });
		m_hdMenu[drv]->addAction(tr("&Eject"), this, [this, drv]() { ejectHd(drv); });
	}

	QMenu *cd = menuBar()->addMenu(tr("&CD"));
	m_cdMenu = cd->addMenu(tr("&CD-ROM"));
	m_cdMenu->addAction(tr("&Open/Mount Image..."), this, &MainWindow::openCd);
	m_cdMenu->addAction(tr("&Eject"), this, &MainWindow::ejectCd);

	QMenu *disp = menuBar()->addMenu(tr("&Display"));
	m_actFullScreen = disp->addAction(tr("&Full Screen"), this, &MainWindow::toggleFullScreen);
	m_actFullScreen->setCheckable(true);
	disp->addSeparator();

	/* Menu order and labels as in the wx port */
	QMenu *rotate = disp->addMenu(tr("&Rotate"));
	auto *rotateGroup = new QActionGroup(this);
	const QString rotateLabels[4] = {
		tr("&Normal"), tr("&Left rotated"), tr("&180 rotated"), tr("&Right rotated"),
	};
	for (int i = 0; i < 4; i++) {
		m_actRotate[i] = rotate->addAction(rotateLabels[i], this, [this, i]() { setRotation(i); });
		m_actRotate[i]->setCheckable(true);
		rotateGroup->addAction(m_actRotate[i]);
	}
	disp->addSeparator();

	QMenu *size = disp->addMenu(tr("&Screen Size"));
	for (int scale = 1; scale <= 3; scale++) {
		size->addAction(tr("&x%1 (%2x%3)").arg(scale).arg(640 * scale).arg(400 * scale),
		                this, [this, scale]() { setScreenScale(scale); });
	}

	QMenu *input = menuBar()->addMenu(tr("&Input"));
	input->addAction(tr("Send Ctrl+Alt+Del"), this, &MainWindow::sendCtrlAltDel);

	/* Menus take keyboard focus; give it back to the screen afterwards */
	for (QAction *a : menuBar()->actions()) {
		if (QMenu *m = a->menu()) {
			connect(m, &QMenu::aboutToHide, m_screen, [this]() { m_screen->setFocus(); });
		}
	}

	updateMenuStatus();
}

/* Menu position of each rotation, as in the wx port */
static const UINT8 kRotateModes[4] = {
	0,
	SCRNMODE_ROTATELEFT,
	SCRNMODE_ROTATEDIR,
	SCRNMODE_ROTATERIGHT,
};

static int rotationIndex()
{
	const UINT8 rot = scrnmode & SCRNMODE_ROTATEMASK;
	for (int i = 0; i < 4; i++) {
		if (kRotateModes[i] == rot) {
			return i;
		}
	}
	return 0;
}

static QString withFileName(const QString &label, const OEMCHAR *path)
{
	if (!path || !path[0]) {
		return label;
	}
	return label + QStringLiteral(": ") + QString::fromUtf8(file_getname(const_cast<OEMCHAR *>(path)));
}

void MainWindow::updateMenuStatus()
{
	for (int i = 0; i < 4; i++) {
		m_fdMenu[i]->setTitle(withFileName(tr("FD &%1").arg(i + 1), np2cfg.fddfile[i]));
		m_hdMenu[i]->setTitle(withFileName(tr("HD &%1").arg(i + 1), diskdrv_getsxsi(static_cast<REG8>(i))));
	}
	const int cd = findCdromDrive();
	m_cdMenu->setTitle(withFileName(tr("&CD-ROM"),
	                   cd >= 0 ? sxsi_getfilename(static_cast<REG8>(cd)) : nullptr));
	m_actFullScreen->setChecked(isFullScreen());
	m_actRotate[rotationIndex()]->setChecked(true);
}

/* ---- Emulate ---- */

void MainWindow::startPause()
{
	m_running = !m_running;
	if (m_running) {
		np2_multithread_resume();
		soundmng_play();
		m_actStartPause->setText(tr("&Pause"));
	} else {
		soundmng_stop();
		np2_multithread_suspend();
		m_actStartPause->setText(tr("&Start"));
	}
}

void MainWindow::reset()
{
	pccore_reset();
	soundmng_reset();
}

void MainWindow::openPreferences()
{
	stopEmulation();
	PrefDialog dialog(this);
	if (dialog.exec() == QDialog::Accepted) {
		if (dialog.needsReset()
		    && QMessageBox::question(this, dialog.windowTitle(),
		           tr("Some of the changes take effect when the PC-98 is reset. Reset it now?"))
		       == QMessageBox::Yes) {
			reset();
		}
		if (dialog.needsRestart()) {
			QMessageBox::information(this, dialog.windowTitle(),
			    tr("The emulation thread setting takes effect the next time the emulator starts."));
		}
		m_screen->update();
	}
	startEmulation();
}

/* ---- drives ---- */

static QString startDir(const char *folder, const OEMCHAR *current)
{
	if (current && current[0]) {
		return QFileInfo(QString::fromUtf8(current)).absolutePath();
	}
	return QString::fromUtf8(folder);
}

/* fdd/sxsi.c: set while a disc swap is pending */
extern "C" char cdchange_flag;

static void storePath(char *dst, const QString &path)
{
	milstr_ncpy(dst, QFile::encodeName(path).constData(), MAX_PATH);
}

void MainWindow::openFd(int drv)
{
	const QString path = QFileDialog::getOpenFileName(this,
	    tr("Open FD%1 Image").arg(drv + 1),
	    startDir(fddfolder, np2cfg.fddfile[drv]),
	    tr("FD images (*.d88 *.88d *.d98 *.98d *.fdi *.hdm *.hd4 *.xdf *.dup *.2hd *.nfd *.dcp *.dcu *.flp *.vfdd);;All files (*)"));
	if (path.isEmpty()) {
		return;
	}
	stopEmulation();
	np2cfg.fddequip |= static_cast<UINT8>(1 << drv);
	fdc.equip       |= static_cast<UINT8>(1 << drv);
	/* Mounted at once, as at startup, not after the usual delay */
	diskdrv_readyfdd(static_cast<REG8>(drv), QFile::encodeName(path).constData(), 0);
	storePath(np2cfg.fddfile[drv], path);
	storePath(fddfolder, QFileInfo(path).absolutePath());
	sysmng_update(SYS_UPDATEFDD | SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

void MainWindow::ejectFd(int drv)
{
	stopEmulation();
	np2cfg.fddfile[drv][0] = '\0';
	fdd_eject(static_cast<REG8>(drv));
	sysmng_update(SYS_UPDATEFDD | SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

void MainWindow::openHd(int drv)
{
	const QString path = QFileDialog::getOpenFileName(this,
	    tr("Open HD%1 Image").arg(drv + 1),
	    startDir(hddfolder, np2cfg.sasihdd[drv]),
	    tr("HD images (*.nhd *.hdi *.thd *.vhd *.hdd *.hdn *.sln);;All files (*)"));
	if (path.isEmpty()) {
		return;
	}
	stopEmulation();
	storePath(hddfolder, QFileInfo(path).absolutePath());
	storePath(np2cfg.sasihdd[drv], path);
#if defined(SUPPORT_IDEIO)
	np2cfg.idetype[drv] = SXSIDEV_HDD;
#endif
	sysmng_update(SYS_UPDATEHDD | SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

void MainWindow::ejectHd(int drv)
{
	stopEmulation();
	np2cfg.sasihdd[drv][0] = '\0';
#if defined(SUPPORT_IDEIO)
	np2cfg.idetype[drv] = SXSIDEV_NC;
#endif
	sysmng_update(SYS_UPDATEHDD | SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

void MainWindow::openCd()
{
	const int drv = findCdromDrive();
	if (drv < 0) {
		QMessageBox::information(this, tr("CD-ROM"), tr("No CD-ROM drive configured."));
		return;
	}
	const OEMCHAR *current = nullptr;
#if defined(SUPPORT_IDEIO)
	current = np2cfg.idecd[drv & 0x03];
#endif
	const QString path = QFileDialog::getOpenFileName(this, tr("Mount CD Image"),
	    startDir(cdfolder, current),
	    tr("CD images (*.iso *.cue *.ccd *.mds *.nrg);;All files (*)"));
	if (path.isEmpty()) {
		return;
	}
	stopEmulation();
	storePath(cdfolder, QFileInfo(path).absolutePath());
	/* With a disc already in, the core ejects it, fails and inserts the new
	 * one a few seconds later; only a failure without that is a real one. */
	if (sxsi_devopen(static_cast<REG8>(drv), QFile::encodeName(path).constData()) != SUCCESS
	    && !cdchange_flag) {
		QMessageBox::warning(this, tr("CD-ROM"),
		    tr("Could not open the CD image:\n%1").arg(QDir::toNativeSeparators(path)));
	} else {
#if defined(SUPPORT_IDEIO)
		storePath(np2cfg.idecd[drv & 0x03], path);
#endif
	}
	sysmng_update(SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

void MainWindow::ejectCd()
{
	const int drv = findCdromDrive();
	if (drv < 0) {
		return;
	}
	stopEmulation();
	sxsi_devopen(static_cast<REG8>(drv), nullptr);
#if defined(SUPPORT_IDEIO)
	np2cfg.idecd[drv & 0x03][0] = '\0';
#endif
	sysmng_update(SYS_UPDATECFG);
	updateMenuStatus();
	startEmulation();
}

/* ---- display / input ---- */

void MainWindow::toggleFullScreen()
{
	/* Full screen hides the menu bar, as wx's ShowFullScreen does; F11 can
	 * bring it back (F11KEY = 1) */
	if (isFullScreen()) {
		showNormal();
		menuBar()->show();
	} else {
		menuBar()->hide();
		showFullScreen();
	}
	m_actFullScreen->setChecked(isFullScreen());
	m_screen->setFocus();
}

void MainWindow::toggleMenuBar()
{
	/* Only in full screen, as in the X port; a window keeps its menu */
	if (isFullScreen()) {
		menuBar()->setVisible(!menuBar()->isVisible());
	}
}

void MainWindow::setRotation(int index)
{
	const UINT8 newmode = static_cast<UINT8>((scrnmode & ~SCRNMODE_ROTATEMASK) | kRotateModes[index]);
	const bool wasPortrait = (scrnmode & SCRNMODE_ROTATE) != 0;
	const bool portrait = (newmode & SCRNMODE_ROTATE) != 0;

	/* changescreen() rebuilds the screen buffer, which the emulator thread
	 * draws into, and restarts the sound behind our back */
	stopEmulation();
	changescreen(newmode);
	if (!m_running) {
		soundmng_stop();
	}
	startEmulation();

	/* Turn the window with the picture, keeping its scale */
	if (portrait != wasPortrait && !isFullScreen() && !isMaximized()) {
		const QSize extra = size() - m_screen->size();
		resize(m_screen->size().transposed() + extra);
	}
	m_screen->update();
	updateMenuStatus();
}

/* Window size for a whole multiple of the PC-98 screen (logical pixels, so
 * HiDPI scaling comes on top, as in the wx port) */
void MainWindow::setScreenScale(int scale)
{
	if (isFullScreen()) {
		toggleFullScreen();
	} else if (isMaximized()) {
		showNormal();
	}
	int w, h;
	scrnmng_getsize(&w, &h);
	QSize target(w * scale, h * scale);
	if (scrnmode & SCRNMODE_ROTATE) {
		target.transpose();
	}
	const QSize extra = size() - m_screen->size();
	resize(target + extra);
}

void MainWindow::sendCtrlAltDel()
{
	keystat_keydown(0x74);  /* CTRL */
	keystat_keydown(0x73);  /* GRPH */
	keystat_keydown(0x39);  /* DEL */
	keystat_keyup(0x39);
	keystat_keyup(0x73);
	keystat_keyup(0x74);
}

/* ---- window state ---- */

void MainWindow::rememberGeometry()
{
	if (isMaximized() || isFullScreen() || !isVisible()) {
		return;
	}
	np2oscfg.winx      = x();
	np2oscfg.winy      = y();
	np2oscfg.winwidth  = static_cast<UINT>(m_screen->width());
	np2oscfg.winheight = static_cast<UINT>(m_screen->height());
}

void MainWindow::moveEvent(QMoveEvent *event)
{
	rememberGeometry();
	QMainWindow::moveEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	rememberGeometry();
	QMainWindow::resizeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	m_screen->pointerCapture()->release();
	m_emuTimer.stop();
	np2_multithread_stop();
	np2_terminate();   /* saves the configuration */
	event->accept();
}
