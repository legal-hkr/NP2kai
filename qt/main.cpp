/* === entry point for the Qt port === */

#include <compiler.h>
#include "mainwindow.h"
#include "np2.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QMenuBar>
#include <QMessageBox>
#include <QSocketNotifier>

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

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral(NP2_WX_APPNAME));
	QApplication::setDesktopFileName(QStringLiteral(NP2_WX_APPNAME));

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("NEC PC-9801 series emulator"));
	parser.addHelpOption();
	QCommandLineOption configOpt({QStringLiteral("c"), QStringLiteral("config")},
	                             QStringLiteral("Configuration file to use."),
	                             QStringLiteral("file"));
	parser.addOption(configOpt);
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

	int exitCode;
	{
		MainWindow window;
		int w = static_cast<int>(np2oscfg.winwidth);
		int h = static_cast<int>(np2oscfg.winheight);
		if (w < 320) w = 640;
		if (h < 200) h = 400;
		/* winwidth/winheight are the screen area; add the menu bar */
		window.resize(w, h + window.menuBar()->sizeHint().height());
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
