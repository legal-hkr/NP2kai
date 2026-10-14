/* === entry point for the Qt port === */

#include <compiler.h>
#include "mainwindow.h"
#include "np2.h"

#include <pccore.h>
#include <fdd/diskdrv.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QMenuBar>
#include <QFileInfo>
#include <QMessageBox>
#include <QProxyStyle>
#include <QStatusBar>
#include <QSocketNotifier>
#include <QStyle>
#include <QStyleOptionMenuItem>

#include <csignal>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>

/* SIGTERM (logout, kill) and SIGINT close the window the normal way, so the
 * configuration and the resume state are saved. The handler only writes to a
 * socket; the event loop does the rest. */
static int s_signalFd[2] = { -1, -1 };

static void onSignal(int)
{
	const char c = 1;
	const ssize_t r = ::write(s_signalFd[0], &c, 1);
	(void)r;
}

static void watchSignals(QWidget *window)
{
	if (::socketpair(AF_UNIX, SOCK_STREAM, 0, s_signalFd) != 0) {
		return;
	}
	auto *notifier = new QSocketNotifier(s_signalFd[1], QSocketNotifier::Read, window);
	QObject::connect(notifier, &QSocketNotifier::activated, window, [window]() {
		char c;
		const ssize_t r = ::read(s_signalFd[1], &c, 1);
		(void)r;
		window->close();
	});
	struct sigaction sa = {};
	sa.sa_handler = onSignal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
}

/* Breeze leaves no room for the submenu arrow when a submenu is the widest
 * item, so the arrow covers the end of its title; the FDD, HDD and CD menus
 * hold nothing else. One line height more fits the arrow in any style. */
class MenuArrowStyle : public QProxyStyle
{
public:
	/* The same style the application chose, wrapped */
	explicit MenuArrowStyle(const QString &key) : QProxyStyle(key) {}

	QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
	                       const QWidget *widget) const override
	{
		QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
		if (type == CT_MenuItem) {
			const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
			if (item && item->menuItemType == QStyleOptionMenuItem::SubMenu) {
				result.rwidth() += item->fontMetrics.height();
			}
		}
		return result;
	}
};

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setStyle(new MenuArrowStyle(QApplication::style()->name()));
	QApplication::setApplicationName(QStringLiteral(NP2_WX_APPNAME));
	QApplication::setDesktopFileName(QStringLiteral(NP2_WX_APPNAME));
	/* The installed icon, as the wx port and the .desktop file use; the copy
	 * built in covers a binary run from its build directory. Under Wayland
	 * the compositor takes the icon from the .desktop file named above. */
	QIcon builtIn;
	for (int size : { 16, 24, 32, 48, 64, 128, 256 }) {
		/* Sizes the X11 window icon can be made in; an SVG names none */
		builtIn.addFile(QStringLiteral(":/np2.svg"), QSize(size, size));
	}
	QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("np2"), builtIn));

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("NEC PC-9801 series emulator"));
	parser.addHelpOption();
	QCommandLineOption configOpt({QStringLiteral("c"), QStringLiteral("config")},
	                             QStringLiteral("Configuration file to use."),
	                             QStringLiteral("file"));
	parser.addOption(configOpt);
	parser.addPositionalArgument(QStringLiteral("images"),
	                             QStringLiteral("Floppy disk images for drives 1 to 4."),
	                             QStringLiteral("[FD1 [FD2 [FD3 [FD4]]]]"));
	parser.process(app);

	/* wx/ini.cpp picks the override up from the environment */
	if (parser.isSet(configOpt)) {
		qputenv("NP2KAI_CONFIG", QFile::encodeName(parser.value(configOpt)));
	}

	/* SDL does audio and joysticks only; the window is Qt's */
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
		QMessageBox::critical(nullptr, QStringLiteral("NP2kai"),
		                      QStringLiteral("SDL_Init failed: %1").arg(QString::fromUtf8(SDL_GetError())));
		return EXIT_FAILURE;
	}

	if (np2_initialize(QFile::encodeName(QCoreApplication::applicationFilePath()).constData()) != SUCCESS) {
		QMessageBox::critical(nullptr, QStringLiteral("NP2kai"),
		                      QStringLiteral("Failed to initialise the emulator core."));
		SDL_Quit();
		return EXIT_FAILURE;
	}

	/* As the X port: in place of the disks the configuration remembers */
	const QStringList images = parser.positionalArguments();
	for (int drv = 0; drv < images.size() && drv < 4; drv++) {
		const QByteArray path = QFile::encodeName(QFileInfo(images[drv]).absoluteFilePath());
		milstr_ncpy(np2cfg.fddfile[drv], path.constData(), NELEMENTS(np2cfg.fddfile[drv]));
		diskdrv_readyfdd(static_cast<REG8>(drv), np2cfg.fddfile[drv], 0);
	}

	int exitCode;
	{
		MainWindow window;
		int w = static_cast<int>(np2oscfg.winwidth);
		int h = static_cast<int>(np2oscfg.winheight);
		if (w < 320) w = 640;
		if (h < 200) h = 400;
		/* winwidth/winheight are the screen area; add the bars */
		window.resize(w, h + window.menuBar()->sizeHint().height()
		                   + window.statusBar()->sizeHint().height());
		if (np2oscfg.winx >= 0 && np2oscfg.winy >= 0) {
			window.move(np2oscfg.winx, np2oscfg.winy);  /* ignored under Wayland */
		}
		window.show();
		watchSignals(&window);
		exitCode = app.exec();
	}

	SDL_Quit();
	return exitCode;
}
