/* === key display window for the Qt port === */

#include <compiler.h>
#include "keydispwindow.h"
#include "mainwindow.h"

#include <generic/cmndraw.h>
#include <generic/keydisp.h>

#include <QActionGroup>
#include <QCloseEvent>
#include <QMenuBar>
#include <QPainter>

/* The colours of the GTK port: background, keys, a note playing */
static const UINT32 kPalette[KEYDISP_PALS] = { 0x000000, 0xffffff, 0xff0000 };

static UINT8 getpal8(CMNPALFN *, UINT num)
{
	return static_cast<UINT8>(num);
}

static UINT32 getpal32(CMNPALFN *, UINT num)
{
	return (num < KEYDISP_PALS) ? kPalette[num] : 0;
}

static UINT16 cnvpal16(CMNPALFN *, RGB32 pal32)
{
	return static_cast<UINT16>(((pal32.p.r >> 3) << 11) | ((pal32.p.g >> 2) << 5) | (pal32.p.b >> 3));
}

/* At about 56 frames a second the PC-98 draws three in 50 ms */
static const int kTickMs = 50;
static const UINT8 kFramesPerTick = 3;

KeyDispWindow::KeyDispWindow(MainWindow *parent)
	: QWidget(parent, Qt::Tool)
	, m_main(parent)
	, m_menuBar(new QMenuBar(this))
	, m_modes(new QActionGroup(this))
{
	setWindowTitle(tr("Key Display"));
	setAttribute(Qt::WA_OpaquePaintEvent);

	QMenu *mode = m_menuBar->addMenu(tr("&Mode"));
	QAction *fm = mode->addAction(tr("&FM, PSG"));
	QAction *midi = mode->addAction(tr("&MIDI"));
	for (QAction *a : { fm, midi }) {
		a->setCheckable(true);
		m_modes->addAction(a);
	}
	fm->setChecked(true);
	connect(fm, &QAction::triggered, this, [this]() { setMode(KEYDISP_MODEFM); });
	connect(midi, &QAction::triggered, this, [this]() { setMode(KEYDISP_MODEMIDI); });

	static bool initialised = false;
	if (!initialised) {
		/* Only the key patterns and the shown channels; the bindings to the
		 * sound chips made at reset stay */
		keydisp_initialize();
		initialised = true;
	}
	static CMNPALFN palfn = { getpal8, getpal32, cnvpal16, 0 };
	keydisp_setpal(&palfn);

	connect(&m_timer, &QTimer::timeout, this, &KeyDispWindow::tick);
}

/* Goes with the main window, after the emulator thread has stopped */
KeyDispWindow::~KeyDispWindow()
{
	keydisp_setmode(KEYDISP_MODENONE);
}

/* The sound chips report their notes from the emulator thread. Reading them
 * here while it runs can at worst show a key late; changing the mode clears
 * the lists it writes to, so that waits for it. */
void KeyDispWindow::switchMode(unsigned char mode)
{
	m_main->stopEmulation();
	keydisp_setmode(mode);
	m_main->startEmulation();
}

void KeyDispWindow::setMode(unsigned char mode)
{
	switchMode(mode);
	resizeToKeys();
}

/* One keyboard per channel in use; the core says when that changes */
void KeyDispWindow::resizeToKeys()
{
	int width, height;
	keydisp_getsize(&width, &height);
	m_image = QImage(width, qMax(height, 1), QImage::Format_RGB32);
	m_image.fill(Qt::black);
	setFixedSize(width, m_menuBar->sizeHint().height() + qMax(height, 1));
	m_menuBar->setFixedWidth(width);

	CMNVRAM vram = {};
	vram.ptr = m_image.bits();
	vram.width = m_image.width();
	vram.height = m_image.height();
	vram.xalign = 4;
	vram.yalign = static_cast<int>(m_image.bytesPerLine());
	vram.bpp = 32;
	keydisp_paint(&vram, TRUE);
	update();
}

void KeyDispWindow::tick()
{
	const UINT8 flag = keydisp_process(kFramesPerTick);
	if (flag & KEYDISP_FLAGSIZING) {
		resizeToKeys();
		return;
	}
	CMNVRAM vram = {};
	vram.ptr = m_image.bits();
	vram.width = m_image.width();
	vram.height = m_image.height();
	vram.xalign = 4;
	vram.yalign = static_cast<int>(m_image.bytesPerLine());
	vram.bpp = 32;
	if (keydisp_paint(&vram, FALSE)) {
		update();
	}
}

void KeyDispWindow::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.drawImage(0, m_menuBar->sizeHint().height(), m_image);
}

void KeyDispWindow::showEvent(QShowEvent *event)
{
	setMode(m_modes->checkedAction() == m_modes->actions().value(1) ? KEYDISP_MODEMIDI : KEYDISP_MODEFM);
	m_timer.start(kTickMs);
	QWidget::showEvent(event);
}

/* Closing only hides it; the notes stop being tracked meanwhile */
void KeyDispWindow::closeEvent(QCloseEvent *event)
{
	m_timer.stop();
	switchMode(KEYDISP_MODENONE);
	Q_EMIT closed();
	QWidget::closeEvent(event);
}
