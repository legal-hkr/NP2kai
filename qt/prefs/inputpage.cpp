/* === preferences: Input page ===
 * The Input tab of wx/dialog/prefframe.cpp, without the settings this port
 * does not read (mouse speed, drag and drop); the joypad device moved to the
 * Joypad page under Sound, next to the game port.
 */

#include <compiler.h>
#include "inputpage.h"
#include "ui_inputpage.h"
#include "np2.h"
#include "kbdmng.h"

#include <pccore.h>

static const UINT8 kKeyboards[] = { KEY_KEY106, KEY_KEY101 };

InputPage::InputPage(QWidget *parent)
	: PrefPage(parent)
	, m_ui(new Ui::InputPage)
{
	m_ui->setupUi(this);
}

InputPage::~InputPage() = default;

void InputPage::load()
{
	setComboValue(m_ui->keyboard, kKeyboards, np2oscfg.KEYBOARD);
	m_ui->F11KEY->setCurrentIndex(np2oscfg.F11KEY < m_ui->F11KEY->count() ? np2oscfg.F11KEY : 0);
	m_ui->F12KEY->setCurrentIndex(np2oscfg.F12KEY < m_ui->F12KEY->count() ? np2oscfg.F12KEY : 0);
	m_ui->XSHIFT->setChecked(np2oscfg.xrollkey != 0);
	m_ui->keyrepeat_enable->setChecked(np2cfg.keyrepeat_enable != 0);
	m_ui->keyrepeat_delay->setValue(np2cfg.keyrepeat_delay);
	m_ui->keyrepeat_interval->setValue(np2cfg.keyrepeat_interval);

	m_ui->btnMODE->setChecked(np2cfg.BTN_MODE != 0);
	m_ui->btnRAPID->setChecked(np2cfg.BTN_RAPID != 0);
	m_ui->MS_RAPID->setChecked(np2cfg.MOUSERAPID != 0);
}

void InputPage::save()
{
	np2oscfg.KEYBOARD = comboValue(m_ui->keyboard, kKeyboards);
	np2oscfg.F11KEY = static_cast<UINT8>(m_ui->F11KEY->currentIndex());
	np2oscfg.F12KEY = static_cast<UINT8>(m_ui->F12KEY->currentIndex());
	np2oscfg.xrollkey = m_ui->XSHIFT->isChecked() ? 1 : 0;
	np2cfg.keyrepeat_enable = m_ui->keyrepeat_enable->isChecked() ? 1 : 0;
	np2cfg.keyrepeat_delay = static_cast<UINT16>(m_ui->keyrepeat_delay->value());
	np2cfg.keyrepeat_interval = static_cast<UINT16>(m_ui->keyrepeat_interval->value());

	np2cfg.BTN_MODE = m_ui->btnMODE->isChecked() ? 1 : 0;
	np2cfg.BTN_RAPID = m_ui->btnRAPID->isChecked() ? 1 : 0;
	np2cfg.MOUSERAPID = m_ui->MS_RAPID->isChecked() ? 1 : 0;
}
